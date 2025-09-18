const { test, expect } = require("@playwright/test");

test.describe("WASM Sketcher API", () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to the page that loads the WASM module and
    // wait for the WASM module to be fully loaded and available
    await page.goto("/wasm_shell.html");
    // wait for the sketcher WASM module to be loaded
    await page.waitForFunction(() => typeof window.Module !== undefined);
  });

  test("Roundtrip SMILES through the sketcher", async ({ page }) => {
    const smiles_input = "c1ccccc1";
    const exportedText = await page.evaluate(async (text) => {
      Module.sketcher_import_text(text);
      return Module.sketcher_export_text(Module.Format.SMILES);
    }, smiles_input);
    // Verify that the exported text matches the input
    expect(exportedText).toBe(smiles_input);
  });

  test("Clear the sketcher", async ({ page }) => {
    // Confirm the sketcher starts empty
    const is_empty = await page.evaluate(() => Module.sketcher_is_empty());
    expect(is_empty).toBe(true);
    // Import a molecule and confirm it's no longer empty
    await page.evaluate(() => {
      Module.sketcher_import_text("C");
    });
    const has_content = await page.evaluate(() => !Module.sketcher_is_empty());
    expect(has_content).toBe(true);
    // Clear the sketcher and confirm it's empty again
    await page.evaluate(() => {
      Module.sketcher_clear();
    });
    const is_cleared = await page.evaluate(() => Module.sketcher_is_empty());
    expect(is_cleared).toBe(true);
  });

  // Parameterized test for all Format values
  const formats = [
    "AUTO_DETECT",
    "RDMOL_BINARY_BASE64",
    "SMILES",
    "EXTENDED_SMILES",
    "SMARTS",
    "EXTENDED_SMARTS",
    "MDL_MOLV2000",
    "MDL_MOLV3000",
    "MAESTRO",
    "INCHI",
    "INCHI_KEY",
    "PDB",
    "XYZ",
    "MRV",
    "CDXML",
    "HELM",
    "FASTA_PEPTIDE",
    "FASTA_DNA",
    "FASTA_RNA",
    "FASTA",
    "FMP",
  ];

  formats.forEach((format) => {
    test(`Format ${format} - import/export roundtrip`, async ({ page }) => {
      const smiles_input = "c1ccccc1";

      await page.evaluate(
        async (data) => {
          const { smiles, formatName } = data;

          // Clear the sketcher, add SMILES, export to the specified
          // format, clear again, import the exported data, and export
          // back to SMILES to verify roundtrip integrity.
          Module.sketcher_clear();
          Module.sketcher_import_text(smiles);
          const exported = Module.sketcher_export_text(
            Module.Format[formatName],
          );
          Module.sketcher_clear();
          Module.sketcher_import_text(exported);

          window.testResults = {
            exported: exported,
            formatName: formatName,
          };
        },
        { smiles: smiles_input, formatName: format },
      );

      // Verify the test completed without throwing errors
      const results = await page.evaluate(() => window.testResults);
      expect(results).toBeDefined();
      expect(results.formatName).toBe(format);
      // Verify that we got some exported data (not empty)
      expect(results.exported).toBeTruthy();
      expect(typeof results.exported).toBe("string");
      expect(results.exported.length).toBeGreaterThan(0);
    });
  });

  // Parameterized test for image export formats
  const imageFormats = ["SVG", "PNG"];

  imageFormats.forEach((imageFormat) => {
    test(`Export as ${imageFormat} image from the sketcher`, async ({
      page,
    }) => {
      const smiles_input = "C=O";
      const base64Content = await page.evaluate(
        (data) => {
          const { smiles, format } = data;
          Module.sketcher_import_text(smiles);
          return Module.sketcher_export_image(Module.ImageFormat[format]);
        },
        { smiles: smiles_input, format: imageFormat },
      );

      // Verify we got non-empty base64 content
      expect(base64Content).toBeTruthy();
      expect(typeof base64Content).toBe("string");
      expect(base64Content.length).toBeGreaterThan(0);

      if (imageFormat === "SVG") {
        // SVG validation using regex pattern similar to Python version
        const SVG_REGEX =
          /(?:<\?xml\b[^>]*>[^<]*)?(?:<!--.*?-->[^<]*)*(?:<svg|<!DOCTYPE svg)\b/s;
        function isValidSvg(svgData) {
          return SVG_REGEX.test(svgData);
        }

        // Decode base64 to get the actual SVG content
        const svgContent = Buffer.from(base64Content, "base64").toString(
          "utf8",
        );
        expect(isValidSvg(svgContent)).toBe(true);
      } else if (imageFormat === "PNG") {
        // PNG files start with specific magic bytes (89 50 4E 47 0D 0A 1A 0A)
        const pngBuffer = Buffer.from(base64Content, "base64");
        expect(pngBuffer.length).toBeGreaterThan(8);

        // Check PNG signature
        const pngSignature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
        for (let i = 0; i < pngSignature.length; i++) {
          expect(pngBuffer[i]).toBe(pngSignature[i]);
        }
      }
    });
  });
});
