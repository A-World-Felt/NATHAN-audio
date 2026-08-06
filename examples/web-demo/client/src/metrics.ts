import type { DemoState } from "./api";

// Flash bref sur toute valeur qui change — meme principe que le flash
// .active de la demo agent-core, pour rester lisible a l'enregistrement
// sans voix off.
function setTextWithFlash(el: HTMLElement, text: string) {
  if (el.textContent === text) return;
  el.textContent = text;
  el.classList.remove("metric-flash");
  // force reflow pour rejouer l'animation meme si elle vient de tourner
  void el.offsetWidth;
  el.classList.add("metric-flash");
}

export function initMetrics(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <section aria-label="Métriques globales">
      <h2>Moteur audio</h2>
      <dl class="metric-list">
        <dt>Profil actif</dt><dd data-field="profile"></dd>
        <dt>Statut HRTF</dt><dd data-field="hrtfStatus"></dd>
        <dt>Renderer</dt><dd data-field="renderer"></dd>
        <dt>Fréquence</dt><dd data-field="sampleRate"></dd>
        <dt>Temps de traitement (moyenne / p95)</dt><dd data-field="frameTime"></dd>
        <dt>Budget par frame</dt><dd data-field="budget"></dd>
        <dt>FPS réel</dt><dd data-field="fps"></dd>
        <dt>Sources simultanées</dt><dd data-field="sources"></dd>
      </dl>
    </section>
    <section aria-label="Métriques par source">
      <h2>Sources</h2>
      <table class="metric-table">
        <thead><tr><th>Source</th><th>Distance</th><th>Azimut</th><th>Gain</th></tr></thead>
        <tbody data-field="sourceRows"></tbody>
      </table>
    </section>
  `;

  const fields = {
    profile: container.querySelector<HTMLElement>('[data-field="profile"]')!,
    hrtfStatus: container.querySelector<HTMLElement>('[data-field="hrtfStatus"]')!,
    renderer: container.querySelector<HTMLElement>('[data-field="renderer"]')!,
    sampleRate: container.querySelector<HTMLElement>('[data-field="sampleRate"]')!,
    frameTime: container.querySelector<HTMLElement>('[data-field="frameTime"]')!,
    budget: container.querySelector<HTMLElement>('[data-field="budget"]')!,
    fps: container.querySelector<HTMLElement>('[data-field="fps"]')!,
    sources: container.querySelector<HTMLElement>('[data-field="sources"]')!,
    sourceRows: container.querySelector<HTMLElement>('[data-field="sourceRows"]')!,
  };

  function render(state: DemoState) {
    setTextWithFlash(
      fields.profile,
      state.profile.usedFallback
        ? `${state.profile.active} (repli : ${state.profile.fallbackReason})`
        : state.profile.active,
    );
    setTextWithFlash(fields.hrtfStatus, state.hrtf.status);
    setTextWithFlash(fields.renderer, `${state.hrtf.renderer} ${state.hrtf.version}`);
    setTextWithFlash(fields.sampleRate, `${state.hrtf.sampleRateHz} Hz`);
    setTextWithFlash(
      fields.frameTime,
      `${state.metrics.frameTimeUs.mean.toFixed(1)} µs / ${state.metrics.frameTimeUs.p95.toFixed(1)} µs`,
    );
    setTextWithFlash(
      fields.budget,
      `${state.metrics.frameBudgetMs.toFixed(1)} ms (${state.metrics.cpuBudgetPercent.toFixed(2)} % utilisé)`,
    );
    setTextWithFlash(fields.fps, state.metrics.fpsReal.toFixed(1));
    setTextWithFlash(
      fields.sources,
      `${state.metrics.sourcesSimultaneous} — R-AUD-03 ${state.metrics.meetsRAud03 ? "OK" : "NON"}`,
    );

    fields.sourceRows.innerHTML = state.sources
      .map(
        (s) => `
          <tr>
            <td>${s.label}</td>
            <td>${s.distanceM.toFixed(1)} m</td>
            <td>${s.azimuthDeg.toFixed(0)}°</td>
            <td>${s.gain.toFixed(2)} (${(20 * Math.log10(Math.max(s.gain, 1e-4))).toFixed(1)} dB)</td>
          </tr>`,
      )
      .join("");
  }

  return { render };
}
