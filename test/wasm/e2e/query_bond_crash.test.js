import { expect, test } from '@playwright/test';
import { Sketcher } from './sketcher_page.js';

test.setTimeout(60_000);

async function requireTestBridge(page) {
  const available = await page.evaluate(() => typeof Module._sketcher_get_rect === 'function');
  test.skip(!available, 'requires a WASM artifact built with playwright_test_bridge.cpp');
}

test.describe('query bond crashes', () => {
  test('survives repeated query bond edits', async ({ page }) => {
    const sk = new Sketcher(page);
    let crashed = false;
    page.on('crash', () => {
      crashed = true;
    });
    await sk.open();
    await requireTestBridge(page);

    await test.step('SKETCH-2399: repeated Single/Double on one bond', async () => {
      await sk.load_structure_for_test('OC1CCCC[C@@H]1Cl');
      await sk.click_tool('single_double', true);
      for (let attempt = 0; attempt < 9; attempt += 1) {
        await sk.click_bond(1);
        expect(crashed).toBe(false);
      }
    });

    await test.step('SKETCH-2409: Up applied twice from the context menu', async () => {
      await sk.click_button('clear');
      await sk.load_structure_for_test('NC(N)=NC(=O)CC1=C(Cl)C=CC=C1Cl');
      const target = { type: 'bond', index: 4 };
      await sk.click_bond(4, true);
      await sk.selection_context_menu(target, 'up');
      await sk.selection_context_menu(target, 'up');
      expect(crashed).toBe(false);
    });

    // A crash tears the page down, so confirm the sketcher still answers.
    expect(await sk.widget_state('undo')).toHaveProperty('enabled', true);
  });
});
