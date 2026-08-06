import { startPolling, type DemoState } from "./api";
import { initScene } from "./scene";
import { initMetrics } from "./metrics";
import { initMp3Panel } from "./mp3panel";
import { initProfileSelector } from "./profileSelector";

const SERVER_URL = import.meta.env.VITE_SERVER_URL ?? "http://127.0.0.1:8787";

const unreachable = document.querySelector<HTMLDivElement>("#unreachable")!;
document.querySelector<HTMLElement>("#unreachableHost")!.textContent = SERVER_URL;

const hrtfDot = document.querySelector<HTMLElement>("#hrtfDot")!;
const hrtfStatus = document.querySelector<HTMLElement>("#hrtfStatus")!;
const profileActive = document.querySelector<HTMLElement>("#profileActive")!;
const sampleRate = document.querySelector<HTMLElement>("#sampleRate")!;

function renderStatusStrip(state: DemoState) {
  hrtfStatus.textContent = state.hrtf.status;
  hrtfDot.classList.toggle("status-dot--good", state.hrtf.status.toLowerCase() === "actif" || state.hrtf.status.toLowerCase() === "enabled");
  hrtfDot.classList.toggle("status-dot--bad", !(state.hrtf.status.toLowerCase() === "actif" || state.hrtf.status.toLowerCase() === "enabled"));
  profileActive.textContent = state.profile.active;
  sampleRate.textContent = `${state.hrtf.sampleRateHz} Hz`;
}

const scene = initScene(document.querySelector<HTMLElement>("#sceneContainer")!);
const metrics = initMetrics(document.querySelector<HTMLElement>("#metrics")!);
const mp3panel = initMp3Panel(document.querySelector<HTMLElement>("#mp3panel")!);
const profileSelector = initProfileSelector(document.querySelector<HTMLElement>("#profileSelector")!);

startPolling(
  (state) => {
    scene.render(state);
    metrics.render(state);
    mp3panel.render(state);
    profileSelector.render(state.profile.active);
    renderStatusStrip(state);
  },
  (reachable) => {
    unreachable.hidden = reachable;
  },
);
