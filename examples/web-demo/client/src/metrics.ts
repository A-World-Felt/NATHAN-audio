import type { DemoState } from "./api";

// Flash bref sur toute valeur qui change — meme principe que le flash
// .active de la demo agent-core, pour rester lisible a l'enregistrement
// sans voix off.
function setTextWithFlash(el: HTMLElement, text: string) {
  if (el.textContent === text) return;
  el.textContent = text;
  el.classList.remove("metric-flash");
  void el.offsetWidth; // force reflow pour rejouer l'animation
  el.classList.add("metric-flash");
}

export function initMetrics(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <div class="stat-grid">
      <div class="stat-tile">
        <span class="stat-value" data-field="frameTime"></span>
        <span class="stat-label">Traitement / frame (moy · p95)</span>
      </div>
      <div class="stat-tile">
        <span class="stat-value" data-field="fps"></span>
        <span class="stat-label">FPS réel</span>
      </div>
      <div class="stat-tile">
        <span class="stat-value" data-field="budget"></span>
        <span class="stat-label">Budget frame utilisé</span>
      </div>
      <div class="stat-tile" data-field="sourcesTile">
        <span class="stat-value" data-field="sources"></span>
        <span class="stat-label">Sources simultanées · R-AUD-03</span>
      </div>
    </div>
    <table class="metric-table">
      <thead><tr><th>Source</th><th>Distance</th><th>Azimut</th><th>Gain</th></tr></thead>
      <tbody data-field="sourceRows"></tbody>
    </table>
  `;

  const fields = {
    frameTime: container.querySelector<HTMLElement>('[data-field="frameTime"]')!,
    fps: container.querySelector<HTMLElement>('[data-field="fps"]')!,
    budget: container.querySelector<HTMLElement>('[data-field="budget"]')!,
    sources: container.querySelector<HTMLElement>('[data-field="sources"]')!,
    sourcesTile: container.querySelector<HTMLElement>('[data-field="sourcesTile"]')!,
    sourceRows: container.querySelector<HTMLElement>('[data-field="sourceRows"]')!,
  };

  function render(state: DemoState) {
    setTextWithFlash(fields.frameTime, `${state.metrics.frameTimeUs.mean.toFixed(0)} / ${state.metrics.frameTimeUs.p95.toFixed(0)} µs`);
    setTextWithFlash(fields.fps, state.metrics.fpsReal.toFixed(0));
    setTextWithFlash(fields.budget, `${state.metrics.cpuBudgetPercent.toFixed(2)} %`);
    setTextWithFlash(fields.sources, `${state.metrics.sourcesSimultaneous} — ${state.metrics.meetsRAud03 ? "OK" : "NON"}`);
    fields.sourcesTile.classList.toggle("stat-tile--ok", state.metrics.meetsRAud03);

    fields.sourceRows.innerHTML = state.sources
      .map((s) => {
        const gainPercent = Math.round(Math.max(0, Math.min(1, s.gain)) * 100);
        const gainDb = (20 * Math.log10(Math.max(s.gain, 1e-4))).toFixed(1);
        return `
          <tr>
            <td class="src-name"><span class="src-dot"></span>${s.label}</td>
            <td>${s.distanceM.toFixed(1)} m</td>
            <td>${s.azimuthDeg.toFixed(0)}°</td>
            <td class="gain-cell">
              <div class="gain-bar"><span style="width:${gainPercent}%"></span></div>
              <span class="gain-value">${s.gain.toFixed(2)} (${gainDb} dB)</span>
            </td>
          </tr>`;
      })
      .join("");
  }

  return { render };
}
