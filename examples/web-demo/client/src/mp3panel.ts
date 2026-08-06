import type { DemoState } from "./api";

export function initMp3Panel(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <div class="mp3-badge" data-field="badge"></div>
    <p class="mp3-note">
      Valeurs réelles décodées par <code>dr_mp3</code>
      (<code>benchmarks/src/mp3_loader.cpp</code>) — pas une estimation.
    </p>
  `;

  const badge = container.querySelector<HTMLElement>('[data-field="badge"]')!;

  function render(state: DemoState) {
    const sizeKo = (state.mp3.fileSizeBytes / 1024).toFixed(1);
    badge.innerHTML = `
      <span><b>${state.mp3.path.split("/").pop()}</b></span>
      <span>${state.mp3.sampleRateHz} Hz</span>
      <span>${state.mp3.channels === 1 ? "mono" : `${state.mp3.channels} canaux`}</span>
      <span>${state.mp3.durationSec.toFixed(1)} s</span>
      <span>${sizeKo} Ko</span>
      <span>${state.mp3.totalSamples.toLocaleString("fr-CA")} échantillons</span>
    `;
  }

  return { render };
}
