import { expect, test } from '@playwright/test';
import {
  clickWidget,
  drawBond,
  focusCanvas,
  getDrawingAreaCenter,
  getExportedSmiles,
  isSketcherEmpty,
  waitForSketcherReady,
} from './e2e_helpers.js';

test.beforeEach(async ({ page }) => {
  await waitForSketcherReady(page);
});

test.describe('canvas editing', () => {
  // Source coverage: tst_erase_mode, tst_move_mode, and tst_select_mode_*.
  test('erase tool removes a placed atom', async ({ page }) => {
    const center = await getDrawingAreaCenter(page);
    await page.mouse.click(center.x, center.y);
    await expect.poll(() => isSketcherEmpty(page)).toBe(false);

    await clickWidget(page, 'erase_btn');
    await page.mouse.click(center.x, center.y);
    await expect.poll(() => isSketcherEmpty(page)).toBe(true);
  });

  test('undo and redo restore a drawn bond', async ({ page }) => {
    await drawBond(page);
    await expect.poll(() => getExportedSmiles(page)).toBe('CC');

    await focusCanvas(page);
    await page.keyboard.press('ControlOrMeta+z');
    await expect.poll(() => isSketcherEmpty(page)).toBe(true);

    await page.keyboard.press('ControlOrMeta+Shift+z');
    await expect.poll(() => getExportedSmiles(page)).toBe('CC');
  });

  test('increase-charge tool changes the selected atom', async ({ page }) => {
    const center = await getDrawingAreaCenter(page);
    await page.mouse.click(center.x, center.y);
    await clickWidget(page, 'increase_charge_btn');
    await page.mouse.click(center.x, center.y);
    await expect.poll(() => getExportedSmiles(page)).toContain('+');
  });
});
