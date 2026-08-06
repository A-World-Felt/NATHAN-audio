import { startPolling, type DemoState } from "./api";
import { initScene } from "./scene";
import { initMetrics } from "./metrics";
import { initMp3Panel } from "./mp3panel";

const app = document.querySelector<HTMLDivElement>("#app")!;
const sceneContainer = document.createElement("div");
sceneContainer.style.width = "600px";
sceneContainer.style.height = "600px";
const metricsContainer = document.createElement("div");
const mp3Container = document.createElement("div");
app.appendChild(sceneContainer);
app.appendChild(metricsContainer);
app.appendChild(mp3Container);

const scene = initScene(sceneContainer);
const metrics = initMetrics(metricsContainer);
const mp3panel = initMp3Panel(mp3Container);

// Fixture temporaire (Task 9, verification visuelle sans backend) — sera
// remplacee par le vrai polling ci-dessous des que web_demo_server tourne.
const fixture: DemoState = {
  player: { x: 1.2, z: -0.6 },
  profile: { active: "subject_003", default: "subject_003", usedFallback: false, fallbackReason: "" },
  hrtf: { status: "Enabled", renderer: "OpenAL Soft", version: "1.1", vendor: "OpenAL Community", sampleRateHz: 44100 },
  sources: [
    { id: "avant", label: "Avant", asset: "assets/test-audio.mp3", format: "mp3", pos: { x: 0, z: -5 }, distanceM: 4.6, azimuthDeg: -12, gain: 0.6 },
    { id: "droite", label: "Droite", asset: "assets/test-audio2.wav", format: "wav", pos: { x: 5, z: 0 }, distanceM: 4.1, azimuthDeg: 73, gain: 0.7 },
    { id: "arriere", label: "Arriere", asset: "assets/test-audio3.wav", format: "wav", pos: { x: 0, z: 5 }, distanceM: 6.8, azimuthDeg: 170, gain: 0.4 },
    { id: "gauche", label: "Gauche", asset: "assets/test-audio4.wav", format: "wav", pos: { x: -5, z: 0 }, distanceM: 7.0, azimuthDeg: -105, gain: 0.35 },
  ],
  metrics: {
    framesProcessed: 100, frameTimeUs: { last: 10, mean: 12, min: 8, max: 20, p95: 18 },
    frameBudgetMs: 15, fpsReal: 66, cpuBudgetPercent: 0.08, sourcesSimultaneous: 4, meetsRAud03: true,
  },
  mp3: { path: "assets/test-audio.mp3", fileSizeBytes: 179837, sampleRateHz: 44100, channels: 1, durationSec: 4.08, totalSamples: 179928 },
};
scene.render(fixture);
metrics.render(fixture);
mp3panel.render(fixture);

startPolling(
  (state) => {
    scene.render(state);
    metrics.render(state);
    mp3panel.render(state);
  },
  () => {},
);
