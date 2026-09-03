import { expect, test } from '@playwright/test';
import { hideMouseMarker, isSketcherEmpty } from './e2e_helpers.js';
import { Sketcher } from './sketcher_page.js';

const SOURCE_STRUCTURE = 'NC(N)=NC(=O)CC1=C(Cl)C=CC=C1Cl';

// Element keys that pick a dedicated toolbar button, and those that instead
// load the element into the shared "last picked element" button.
const ELEMENT_KEYS = ['f', 'h', 'n', 'o', 'p', 's'];
const PERIODIC_TABLE_KEYS = ['i', 'b', 'k', 'u'];
const ALL_ELEMENT_KEYS = [...ELEMENT_KEYS, ...PERIODIC_TABLE_KEYS];

// Bond order keys, in the order the source presses them.
const BOND_KEYS = [
  ['0', 'zero'],
  ['2', 'double'],
  ['3', 'triple'],
  ['1', 'single'],
];

test.setTimeout(300_000);

async function requireTestBridge(page) {
  const available = await page.evaluate(() => typeof Module._sketcher_get_rect === 'function');
  test.skip(!available, 'requires a WASM artifact built with playwright_test_bridge.cpp');
}

async function checkpoint(page, name) {
  await page.mouse.move(0, 0);
  await hideMouseMarker(page);
  await expect(page.locator('#screen canvas')).toHaveScreenshot(`${name}.png`);
}

async function restart(sk) {
  await sk.reset_state();
  await sk.load_structure_for_test(SOURCE_STRUCTURE);
}

async function selectSourceItems(sk) {
  for (const index of [1, 4, 7]) {
    await sk.click_atom(index, true, 'shift');
    await sk.click_bond(index, true, 'shift');
  }
}

test.describe('hidden shortcuts', () => {
  test('keyboard shortcuts pick tools and edit the structure', async ({ page }) => {
    const sk = new Sketcher(page);
    await sk.open();
    await requireTestBridge(page);

    await test.step('shortcuts pick a tool with nothing on the canvas', async () => {
      for (const key of ALL_ELEMENT_KEYS) {
        await sk.type_text('sketcher_area', key);
        const element = key.toUpperCase();
        const usesPeriodicTable = PERIODIC_TABLE_KEYS.includes(key);
        const state = await sk.widget_state(
          usesPeriodicTable ? 'last_picked_element_btn' : element,
        );
        expect(state.enabled, `${element} enabled`).toBe(true);
        expect(state.checked, `${element} checked`).toBe(true);
        if (usesPeriodicTable) {
          expect(state.text).toBe(element);
        }
      }
      for (const [key, tool] of BOND_KEYS) {
        await sk.type_text('sketcher_area', key);
        const state = await sk.widget_state(tool);
        expect(state.enabled, `bond ${key} enabled`).toBe(true);
        expect(state.checked, `bond ${key} checked`).toBe(true);
      }
    });

    await test.step('a shortcut changes the atom under the pointer', async () => {
      await restart(sk);
      const atom1 = await sk.rendered_object_rect('atom', 1);
      for (const key of [...ALL_ELEMENT_KEYS, '-', '=']) {
        await sk.hover_rendered_target(atom1);
        await sk.type_text('sketcher_area', key);
        await checkpoint(page, `change-atom-hover-${key.toUpperCase()}`);
      }
    });

    await test.step('a shortcut changes the bond under the pointer', async () => {
      await restart(sk);
      const bond4 = await sk.rendered_object_rect('bond', 4);
      for (const [key] of BOND_KEYS) {
        await sk.hover_rendered_target(bond4);
        await sk.type_text('sketcher_area', key);
        await checkpoint(page, `change-bond-hover-${key}`);
      }
    });

    await test.step('a shortcut edits the whole selection', async () => {
      await restart(sk);
      await selectSourceItems(sk);
      for (const key of [...ALL_ELEMENT_KEYS, 'd', 't', 'c']) {
        await sk.type_text('sketcher_area', key);
        await checkpoint(page, `change-atoms-selection-${key.toUpperCase()}`);
      }
      for (const [key] of BOND_KEYS) {
        await sk.type_text('sketcher_area', key);
        await checkpoint(page, `change-bond-order-${key}`);
      }
      // Charge steps up three times and then back down five, so that the
      // selection passes through neutral and goes negative.
      const charges = ['+', '-', '+', '+', '-', '-', '-', '-'];
      for (const [step, key] of charges.entries()) {
        await sk.type_text('sketcher_area', key);
        await checkpoint(page, `charge-${step + 1}-${key === '+' ? 'up' : 'down'}`);
      }
    });

    await test.step('Backspace picks Erase, then deletes what is hovered', async () => {
      await restart(sk);
      await sk.click_tool('rect_btn');
      await sk.click_sketcher(-500, 500);
      await sk.type_text('sketcher_area', '<Backspace>');
      expect((await sk.widget_state('erase')).checked).toBe(true);
      await sk.hover_rendered_target(await sk.rendered_object_rect('atom', 1));
      await sk.type_text('sketcher_area', '<Backspace>');
      await checkpoint(page, 'delete-atom-hover');
    });

    await test.step('Backspace steps a bond down one order at a time', async () => {
      await restart(sk);
      const bond6 = await sk.rendered_object_rect('bond', 6);
      await sk.click_tool('triple', true);
      await sk.click_rendered_target(bond6);
      for (const step of [1, 2, 3]) {
        await sk.hover_rendered_target(bond6);
        await sk.type_text('sketcher_area', '<Backspace>');
        await checkpoint(page, `delete-bond-hover-${step}`);
      }
    });

    await test.step('Backspace deletes the selection, and Space returns to select', async () => {
      await restart(sk);
      await selectSourceItems(sk);
      await sk.type_text('sketcher_area', '<Backspace>');
      await checkpoint(page, 'delete-selection');
      // Only the selection goes, so the rest of the structure stays behind.
      await expect.poll(() => isSketcherEmpty(page)).toBe(false);

      // Space leaves any drawing tool for the current selection tool, whichever
      // shape that tool is currently set to.
      for (const shape of ['rect', 'lasso']) {
        await sk.click_tool(shape, true);
        for (const tool of ['C', 'move_rotate', 'erase']) {
          await sk.click_tool(tool);
          await sk.type_text('sketcher_area', ' ');
          expect((await sk.widget_state('select_tool_btn')).checked, `${shape} after ${tool}`).toBe(
            true,
          );
        }
      }
    });
  });
});
