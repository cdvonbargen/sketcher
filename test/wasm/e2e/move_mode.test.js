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

test.describe('move/rotate mode', () => {
  test('moves and rotates a selection, and pans the canvas', async ({ page }) => {
    const sk = new Sketcher(page);
    await sk.open();
    await requireTestBridge(page);
    await sk.load_structure_for_test(SOURCE_STRUCTURE);

    for (const n of [1, 4, 7]) {
      await sk.click_bond(n, true, 'shift');
      await sk.click_atom(n, true, 'shift');
    }
    await sk.click_tool('move_rotate');
    await checkpoint(page, 'maintain-previous-selection');

    const atom4 = await sk.rendered_object_rect('atom', 4);
    const atom3 = await sk.rendered_object_rect('atom', 3);

    await test.step('drag the selection', async () => {
      await sk.drag_from_rendered_target(atom4, 50, 50);
      await checkpoint(page, 'drag-selection');
      await sk.click_button('undo');
    });

    await test.step('rotate the selection', async () => {
      // The rotation handle sits out to the side of the selection, so the drag
      // starts clear of the atoms rather than on one of them.
      await sk.drag(sk.center_of(atom4, 130, 0), sk.center_of(atom3));
      await checkpoint(page, 'rotate-selection');
      await sk.click_button('undo');
    });

    await test.step('drag the background to pan', async () => {
      await sk.mouse_drag(10, 10, 100, 100);
      await checkpoint(page, 'drag-background');
    });

    await test.step('a second structure moves independently', async () => {
      await sk.click_button('clear_selection');
      await sk.click_tool('rect_btn');
      // Loading adds to the canvas, so this leaves two copies on it.
      await sk.load_structure_for_test(SOURCE_STRUCTURE);
      await sk.click_button('cleanup');

      const secondStructureAtom = await sk.rendered_object_rect('atom', 18);
      await sk.click_tool('move_rotate');
      await sk.drag_from_rendered_target(secondStructureAtom, 30, -40);
      await checkpoint(page, 'drag-one-of-two-structures');
      await sk.click_button('undo');

      await sk.mouse_drag(10, 10, 100, 100);
      await checkpoint(page, 'drag-background-two-structures');
    });
  });
});
