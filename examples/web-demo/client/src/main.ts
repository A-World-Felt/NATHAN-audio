import { startPolling } from "./api";
import { initScene } from "./scene";
import { initMetrics } from "./metrics";
import { initMp3Panel } from "./mp3panel";
import { initProfileSelector } from "./profileSelector";

const SERVER_URL = import.meta.env.VITE_SERVER_URL ?? "http://127.0.0.1:8787";

const unreachable = document.querySelector<HTMLDivElement>("#unreachable")!;
document.querySelector<HTMLElement>("#unreachableHost")!.textContent = SERVER_URL;

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
  },
  (reachable) => {
    unreachable.hidden = reachable;
  },
);
