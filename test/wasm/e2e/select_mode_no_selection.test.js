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

test.describe('select mode with no selection', () => {
  test('selects atoms and bonds by click and by drag', async ({ page }) => {
    const sk = new Sketcher(page);
    await sk.open();
    await requireTestBridge(page);
    await sk.load_structure_for_test(SOURCE_STRUCTURE);

    await test.step('selection actions stay disabled with nothing selected', async () => {
      // All three selection shapes live in the select tool's popup. The button
      // itself starts out as rect, so only the other two need the popup.
      for (const [tool, in_popup] of [
        ['rect_btn', false],
        ['ellipse', true],
        ['lasso', true],
      ]) {
        await sk.click_tool(tool, in_popup);
        expect((await sk.widget_state('clear_selection')).enabled).toBe(false);
        expect((await sk.widget_state('invert_selection')).enabled).toBe(false);
      }
    });

    // The select tool button now stands for lasso, so rect has to come back
    // from the popup rather than from a plain click on the button.
    await sk.click_tool('rect', true);
    for (const n of [1, 4, 7]) {
      await test.step(`click atom ${n}, then bond ${n}`, async () => {
        await sk.click_atom(n, true);
        await checkpoint(page, `select-atom-${n}`);
        await sk.click_bond(n, true);
        await checkpoint(page, `select-bond-${n}`);
      });
    }

    await test.step('shift-click adds to the selection', async () => {
      await sk.click_button('clear_selection');
      for (const n of [1, 4, 7]) {
        await sk.click_atom(n, true, 'shift');
      }
      await checkpoint(page, 'add-atoms-to-selection');
      await sk.click_button('clear_selection');
      for (const n of [1, 4, 7]) {
        await sk.click_bond(n, true, 'shift');
      }
      await checkpoint(page, 'add-bonds-to-selection');
      await sk.click_button('clear_selection');
      for (const n of [1, 4, 7]) {
        await sk.click_atom(n, true, 'shift');
        await sk.click_bond(n, true, 'shift');
      }
      await checkpoint(page, 'add-both-to-selection');
    });

    await test.step('ctrl-click inverts individual items', async () => {
      for (const n of [1, 4, 7]) {
        await sk.click_bond(n, true, 'control');
        await checkpoint(page, `invert-bond-${n}`);
        await sk.click_atom(n, true, 'control');
        await checkpoint(page, `invert-atom-${n}`);
      }
    });

    const oxygen = await sk.rendered_object_rect('atom', 6);
    const chlorine = await sk.rendered_object_rect('atom', 15);
    const nitrogen1 = await sk.rendered_object_rect('atom', 1);
    const nitrogen4 = await sk.rendered_object_rect('atom', 4);

    await test.step('drag a selection rectangle', async () => {
      await sk.click_button('clear_selection');
      await sk.drag_between_rendered_targets(oxygen, chlorine);
      await checkpoint(page, 'mouse-drag');
      await sk.click_button('clear_selection');
      await sk.drag_between_rendered_targets(oxygen, chlorine, 'shift');
      await checkpoint(page, 'shift-drag');
      await sk.drag_between_rendered_targets(nitrogen1, chlorine, 'control');
      await checkpoint(page, 'ctrl-drag');
    });

    await test.step('double-click selects the whole molecule', async () => {
      await sk.click_button('clear_selection');
      await sk.double_click_rendered_target(nitrogen1);
      await checkpoint(page, 'double-click');
    });

    await test.step('move/rotate drags the canvas instead of selecting', async () => {
      await sk.click_button('clear_selection');
      await sk.click_tool('move_rotate');
      await sk.drag_between_rendered_targets(nitrogen1, nitrogen4);
      await checkpoint(page, 'drag-canvas');
    });
  });
});
