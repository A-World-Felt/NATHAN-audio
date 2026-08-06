import type { DemoState } from "./api";

export function initMp3Panel(container: HTMLElement): { render(state: DemoState): void } {
  container.innerHTML = `
    <section aria-label="Décodage MP3">
      <h2>Décodage MP3</h2>
      <p class="mp3-note">
        Valeurs réelles décodées par <code>dr_mp3</code>
        (<code>benchmarks/src/mp3_loader.cpp</code>), pas une estimation.
      </p>
      <dl class="metric-list">
        <dt>Fichier</dt><dd data-field="path"></dd>
        <dt>Taille</dt><dd data-field="size"></dd>
        <dt>Fréquence décodée</dt><dd data-field="rate"></dd>
        <dt>Canaux</dt><dd data-field="channels"></dd>
        <dt>Durée</dt><dd data-field="duration"></dd>
        <dt>Échantillons décodés</dt><dd data-field="samples"></dd>
      </dl>
    </section>
  `;

  const fields = {
    path: container.querySelector<HTMLElement>('[data-field="path"]')!,
    size: container.querySelector<HTMLElement>('[data-field="size"]')!,
    rate: container.querySelector<HTMLElement>('[data-field="rate"]')!,
    channels: container.querySelector<HTMLElement>('[data-field="channels"]')!,
    duration: container.querySelector<HTMLElement>('[data-field="duration"]')!,
    samples: container.querySelector<HTMLElement>('[data-field="samples"]')!,
  };

  function render(state: DemoState) {
    fields.path.textContent = state.mp3.path;
    fields.size.textContent = `${(state.mp3.fileSizeBytes / 1024).toFixed(1)} Ko`;
    fields.rate.textContent = `${state.mp3.sampleRateHz} Hz`;
    fields.channels.textContent = String(state.mp3.channels);
    fields.duration.textContent = `${state.mp3.durationSec.toFixed(2)} s`;
    fields.samples.textContent = state.mp3.totalSamples.toLocaleString("fr-CA");
  }

  return { render };
}
