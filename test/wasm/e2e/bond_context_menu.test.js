import { expect, test } from '@playwright/test';
import { hideMouseMarker } from './e2e_helpers.js';
import { Sketcher } from './sketcher_page.js';

const SOURCE_STRUCTURE = 'NC(N)=NC(=O)CC1=C(Cl)C=CC=C1Cl';

// Every row of the bond context menu in one stateful walk, so this needs longer
// than the default per-test timeout.
test.setTimeout(180_000);

async function requireTestBridge(page) {
  const available = await page.evaluate(() => typeof Module._sketcher_get_rect === 'function');
  test.skip(!available, 'requires a WASM artifact built with playwright_test_bridge.cpp');
}

async function checkpoint(page, name) {
  await page.mouse.move(0, 0);
  await hideMouseMarker(page);
  await expect(page.locator('#screen canvas')).toHaveScreenshot(`${name}.png`);
}

test.describe('bond context menu', () => {
  test('applies each bond type from the context menu', async ({ page }) => {
    const sk = new Sketcher(page);
    await sk.open();
    await requireTestBridge(page);

    // Source setup is Import -> Paste in Text, then read back coordinates to
    // locate bonds. The bridge returns live rendered geometry instead, so the
    // fixture only has to establish the same starting structure.
    await sk.load_structure_for_test(SOURCE_STRUCTURE);
    const target = { type: 'bond', index: 4 };

    await test.step('flip_substituent', async () => {
      await sk.click_bond(4, true);
      await sk.selection_context_menu(target, 'flip_substituent');
      await checkpoint(page, 'flip-substituent');
    });

    // Source: select bonds one through six before the remaining rows.
    for (const index of [1, 2, 3, 4, 5, 6]) {
      await sk.click_bond(index, true, 'shift');
    }

    for (const action of ['double', 'single', 'aromatic', 'up', 'down']) {
      await test.step(action, async () => {
        await sk.selection_context_menu(target, action);
        await checkpoint(page, action);
      });
    }

    await test.step('delete', async () => {
      // Change back to single bonds before deletion, exactly as in Squish.
      await sk.selection_context_menu(target, 'single');
      await sk.selection_context_menu(target, 'delete');
      await checkpoint(page, 'delete');
      await sk.click_button('undo');
    });

    for (const action of ['Coordinate', 'Zero Order', 'Single Up/Down', 'Double Cis/Trans']) {
      await test.step(`other_type: ${action}`, async () => {
        await sk.selection_context_menu(target, 'other_type', action);
        await checkpoint(page, action.replace(/\//g, '-'));
      });
    }

    for (const action of ['Any', 'Single/Double', 'Double/Aromatic', 'Single/Aromatic']) {
      await test.step(`query: ${action}`, async () => {
        await sk.selection_context_menu(target, 'query', action);
        await checkpoint(page, action.replace(/\//g, '-'));
      });
    }

    // Source: return to single bonds before the topology rows.
    await sk.selection_context_menu(target, 'single');

    for (const action of ['In Ring', 'Not In a Ring', 'Either']) {
      await test.step(`topology: ${action}`, async () => {
        await sk.selection_context_menu(target, 'topology', action);
        await checkpoint(page, action);
      });
    }
  });
});
