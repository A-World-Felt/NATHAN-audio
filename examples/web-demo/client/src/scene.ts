import { postPlayer, type DemoState, type SourceState } from "./api";

const VIEW_MIN = -12;
const VIEW_MAX = 12;
const VIEW_SIZE = VIEW_MAX - VIEW_MIN;
const DRAG_POST_THROTTLE_MS = 50;

// Convention (docs/superpowers/specs/2026-08-06-web-audio-demo-design.md) :
// haut ecran = avant (-Z), droite ecran = droite (+X). Le SVG a Y vers le
// bas, donc screenY = -worldZ (inverse de Z) pour que "avant" reste en haut.
function worldToScreen(x: number, z: number): { sx: number; sy: number } {
  return { sx: x, sy: -z };
}

function svgEl<K extends keyof SVGElementTagNameMap>(tag: K): SVGElementTagNameMap[K] {
  return document.createElementNS("http://www.w3.org/2000/svg", tag);
}

export function initScene(container: HTMLElement): { render(state: DemoState): void } {
  const svg = svgEl("svg");
  svg.setAttribute("viewBox", `${VIEW_MIN} ${VIEW_MIN} ${VIEW_SIZE} ${VIEW_SIZE}`);
  svg.setAttribute("class", "scene-svg");
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "Scène spatiale : joueur, 4 sources sonores et vecteurs de position relative");

  const gridGroup = svgEl("g");
  gridGroup.setAttribute("class", "scene-grid");
  for (let i = VIEW_MIN; i <= VIEW_MAX; i++) {
    const isMajor = i % 5 === 0;
    const vLine = svgEl("line");
    vLine.setAttribute("x1", String(i));
    vLine.setAttribute("x2", String(i));
    vLine.setAttribute("y1", String(VIEW_MIN));
    vLine.setAttribute("y2", String(VIEW_MAX));
    vLine.setAttribute("class", isMajor ? "grid-line grid-line--major" : "grid-line");
    gridGroup.appendChild(vLine);

    const hLine = svgEl("line");
    hLine.setAttribute("x1", String(VIEW_MIN));
    hLine.setAttribute("x2", String(VIEW_MAX));
    hLine.setAttribute("y1", String(i));
    hLine.setAttribute("y2", String(i));
    hLine.setAttribute("class", isMajor ? "grid-line grid-line--major" : "grid-line");
    gridGroup.appendChild(hLine);
  }
  svg.appendChild(gridGroup);

  const vectorsGroup = svgEl("g");
  vectorsGroup.setAttribute("class", "scene-vectors");
  svg.appendChild(vectorsGroup);

  const spritesGroup = svgEl("g");
  spritesGroup.setAttribute("class", "scene-sprites");
  svg.appendChild(spritesGroup);

  const playerCircle = svgEl("circle");
  playerCircle.setAttribute("class", "scene-player");
  playerCircle.setAttribute("r", "0.5");
  playerCircle.setAttribute("tabindex", "0");
  playerCircle.setAttribute("role", "slider");
  playerCircle.setAttribute("aria-label", "Joueur — déplacer à la souris");
  svg.appendChild(playerCircle);

  container.appendChild(svg);

  let latestSources: SourceState[] = [];
  let dragging = false;
  let lastPostAt = 0;

  function clientToWorld(clientX: number, clientY: number): { x: number; z: number } {
    const rect = svg.getBoundingClientRect();
    const sx = VIEW_MIN + ((clientX - rect.left) / rect.width) * VIEW_SIZE;
    const sy = VIEW_MIN + ((clientY - rect.top) / rect.height) * VIEW_SIZE;
    return { x: sx, z: -sy };
  }

  function onPointerMove(ev: PointerEvent) {
    if (!dragging) return;
    const { x, z } = clientToWorld(ev.clientX, ev.clientY);
    const clampedX = Math.max(-10, Math.min(10, x));
    const clampedZ = Math.max(-10, Math.min(10, z));
    const { sx, sy } = worldToScreen(clampedX, clampedZ);
    playerCircle.setAttribute("cx", String(sx));
    playerCircle.setAttribute("cy", String(sy));

    const now = performance.now();
    if (now - lastPostAt >= DRAG_POST_THROTTLE_MS) {
      lastPostAt = now;
      void postPlayer(clampedX, clampedZ);
    }
  }

  playerCircle.addEventListener("pointerdown", (ev) => {
    dragging = true;
    playerCircle.setPointerCapture(ev.pointerId);
  });
  playerCircle.addEventListener("pointerup", (ev) => {
    dragging = false;
    playerCircle.releasePointerCapture(ev.pointerId);
  });
  svg.addEventListener("pointermove", onPointerMove);

  function render(state: DemoState) {
    latestSources = state.sources;

    if (!dragging) {
      const { sx, sy } = worldToScreen(state.player.x, state.player.z);
      playerCircle.setAttribute("cx", String(sx));
      playerCircle.setAttribute("cy", String(sy));
    }

    while (spritesGroup.firstChild) spritesGroup.removeChild(spritesGroup.firstChild);
    while (vectorsGroup.firstChild) vectorsGroup.removeChild(vectorsGroup.firstChild);

    const playerScreen = worldToScreen(state.player.x, state.player.z);

    for (const source of latestSources) {
      const { sx, sy } = worldToScreen(source.pos.x, source.pos.z);

      const vector = svgEl("line");
      vector.setAttribute("x1", String(playerScreen.sx));
      vector.setAttribute("y1", String(playerScreen.sy));
      vector.setAttribute("x2", String(sx));
      vector.setAttribute("y2", String(sy));
      vector.setAttribute("class", "scene-vector");
      vector.setAttribute("stroke-opacity", String(Math.max(0.25, source.gain)));
      vectorsGroup.appendChild(vector);

      const label = svgEl("text");
      label.setAttribute("x", String((playerScreen.sx + sx) / 2));
      label.setAttribute("y", String((playerScreen.sy + sy) / 2));
      label.setAttribute("class", "scene-vector-label");
      label.textContent = `${source.distanceM.toFixed(1)} m · ${source.azimuthDeg.toFixed(0)}°`;
      vectorsGroup.appendChild(label);

      const sprite = svgEl("circle");
      sprite.setAttribute("cx", String(sx));
      sprite.setAttribute("cy", String(sy));
      sprite.setAttribute("r", "0.4");
      sprite.setAttribute("class", "scene-sprite");
      spritesGroup.appendChild(sprite);

      const spriteLabel = svgEl("text");
      spriteLabel.setAttribute("x", String(sx));
      spriteLabel.setAttribute("y", String(sy - 0.6));
      spriteLabel.setAttribute("class", "scene-sprite-label");
      spriteLabel.textContent = `${source.label} (${source.format})`;
      spritesGroup.appendChild(spriteLabel);
    }
  }

  return { render };
}
