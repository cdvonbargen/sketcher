import { expect, test } from '@playwright/test';
import { hideMouseMarker } from './e2e_helpers.js';
import { Sketcher } from './sketcher_page.js';

const SOURCE_STRUCTURE = 'NC(N)=NC(=O)CC1=C(Cl)C=CC=C1Cl';

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

test.describe('erase mode', () => {
  test('erases atoms and bonds, and steps bond order down', async ({ page }) => {
    const sk = new Sketcher(page);
    await sk.open();
    await requireTestBridge(page);
    await sk.load_structure_for_test(SOURCE_STRUCTURE);

    // Erasing renumbers everything after the erased item, so resolve all the
    // targets before the first click and then click the recorded points.
    const atoms = await sk.capture_rendered_targets('atom', [1, 3, 5, 7]);
    const bonds = await sk.capture_rendered_targets('bond', [1, 3, 5, 7]);

    await sk.click_tool('erase');
    await test.step('delete atoms', async () => {
      for (const atom of [3, 5, 7]) {
        await sk.click_rendered_target(atoms.get(atom));
      }
      await checkpoint(page, 'delete-atoms');
    });
    await test.step('delete bonds', async () => {
      for (const bond of [3, 5, 7]) {
        await sk.click_rendered_target(bonds.get(bond));
      }
      await checkpoint(page, 'delete-bonds');
    });
    await test.step('drag to delete', async () => {
      await sk.drag_from_rendered_target(atoms.get(1), -600, 200);
      await checkpoint(page, 'mouse-drag-delete');
    });

    await sk.click_button('clear');
    await sk.load_structure_for_test(SOURCE_STRUCTURE);

    // Erase steps a bond down one order at a time before removing it, so the
    // same point is clicked repeatedly.
    const bond1 = await sk.rendered_object_rect('bond', 1);
    await sk.click_tool('triple', true);
    await sk.click_rendered_target(bond1);
    await checkpoint(page, 'change-to-triple');

    await sk.click_tool('erase');
    for (const name of [
      'erase-triple-to-double',
      'erase-double-to-single',
      'erase-single-to-none',
    ]) {
      await test.step(name, async () => {
        await sk.click_rendered_target(bond1);
        await checkpoint(page, name);
      });
    }
  });
});
