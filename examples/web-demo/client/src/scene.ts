import { postPlayer, type DemoState, type SourceState } from "./api";

const VIEW_MIN = -12;
const VIEW_MAX = 12;
const VIEW_SIZE = VIEW_MAX - VIEW_MIN;
const DRAG_POST_THROTTLE_MS = 50;
const KEYBOARD_STEP_M = 0.5;
const CLAMP_M = 10;
const RING_STEP_M = 2;

// Convention (docs/superpowers/specs/2026-08-06-web-audio-demo-design.md,
// OPENAL_SOFT_NATHAN.md #4) : avant = -Z, arriere = +Z, droite = +X.
// screenY croit vers le bas en SVG, donc screenY = worldZ tel quel place
// deja l'avant (Z negatif) en haut et l'arriere (Z positif) en bas — AUCUNE
// inversion de signe necessaire. Verifie empiriquement (Playwright) : avec
// sy=-z (version precedente), "Avant" et "Arriere" rendaient a l'envers des
// libelles d'axe.
function worldToScreen(x: number, z: number): { sx: number; sy: number } {
  return { sx: x, sy: z };
}

function svgEl<K extends keyof SVGElementTagNameMap>(tag: K): SVGElementTagNameMap[K] {
  return document.createElementNS("http://www.w3.org/2000/svg", tag);
}

function clamp(v: number, lo: number, hi: number): number {
  return Math.max(lo, Math.min(hi, v));
}

// Etiquette avec un fond plein (pill) pour rester lisible par-dessus les
// anneaux/vecteurs, plutot qu'un texte flottant qui se fond dans la scene.
function labelWithBackground(text: string, x: number, y: number, cls: string): SVGGElement {
  const g = svgEl("g");
  const t = svgEl("text");
  t.setAttribute("x", String(x));
  t.setAttribute("y", String(y));
  t.setAttribute("class", cls);
  t.textContent = text;
  const bg = svgEl("rect");
  bg.setAttribute("class", "scene-label-bg");
  // dimensionne le fond apres insertion (getBBox n'est fiable qu'une fois
  // le texte attache au DOM).
  g.appendChild(bg);
  g.appendChild(t);
  requestAnimationFrame(() => {
    try {
      const box = t.getBBox();
      bg.setAttribute("x", String(box.x - 0.15));
      bg.setAttribute("y", String(box.y - 0.06));
      bg.setAttribute("width", String(box.width + 0.3));
      bg.setAttribute("height", String(box.height + 0.12));
    } catch {
      // getBBox peut lever si l'element n'est pas rendu (onglet en arriere-plan) ; sans consequence visuelle.
    }
  });
  return g;
}

export function initScene(container: HTMLElement): { render(state: DemoState): void } {
  const svg = svgEl("svg");
  svg.setAttribute("viewBox", `${VIEW_MIN} ${VIEW_MIN} ${VIEW_SIZE} ${VIEW_SIZE}`);
  svg.setAttribute("class", "scene-svg");
  svg.setAttribute("role", "img");
  svg.setAttribute("aria-label", "Scène spatiale : joueur, 4 sources sonores et vecteurs de position relative");

  // --- Anneaux de distance + axes cardinaux (rendu statique, une fois) ---
  // Etiquettes de distance placees a 35 deg de l'axe arriere (hors des 4
  // axes cardinaux ou vivent les sprites/etiquettes d'axe) pour ne jamais
  // les recouvrir, quel que soit le nombre d'anneaux. Cote bas-droit
  // (quadrant ARRIERE/DROITE) et non haut-droit : la legende fixe (voir
  // .scene-legend) occupe le coin haut-droit de la scene, et les etiquettes
  // des anneaux exterieurs (10 m, 12 m) s'y retrouvaient cachees dessous —
  // trouve en revue independante round 2.
  const RING_LABEL_ANGLE_RAD = (145 * Math.PI) / 180;
  const ringsGroup = svgEl("g");
  ringsGroup.setAttribute("class", "scene-rings");
  for (let r = RING_STEP_M; r <= VIEW_MAX; r += RING_STEP_M) {
    const ring = svgEl("circle");
    ring.setAttribute("cx", "0");
    ring.setAttribute("cy", "0");
    ring.setAttribute("r", String(r));
    ring.setAttribute("class", "scene-ring");
    ringsGroup.appendChild(ring);

    const label = svgEl("text");
    label.setAttribute("x", String(r * Math.sin(RING_LABEL_ANGLE_RAD)));
    label.setAttribute("y", String(r * -Math.cos(RING_LABEL_ANGLE_RAD)));
    label.setAttribute("class", "scene-ring-label");
    label.textContent = `${r} m`;
    ringsGroup.appendChild(label);
  }
  svg.appendChild(ringsGroup);

  const axisGroup = svgEl("g");
  axisGroup.setAttribute("class", "scene-axes");
  const vAxis = svgEl("line");
  vAxis.setAttribute("x1", "0"); vAxis.setAttribute("x2", "0");
  vAxis.setAttribute("y1", String(VIEW_MIN)); vAxis.setAttribute("y2", String(VIEW_MAX));
  vAxis.setAttribute("class", "scene-axis");
  const hAxis = svgEl("line");
  hAxis.setAttribute("x1", String(VIEW_MIN)); hAxis.setAttribute("x2", String(VIEW_MAX));
  hAxis.setAttribute("y1", "0"); hAxis.setAttribute("y2", "0");
  hAxis.setAttribute("class", "scene-axis");
  axisGroup.appendChild(vAxis);
  axisGroup.appendChild(hAxis);

  const axisLabels: Array<[string, number, number]> = [
    ["AVANT", 0, VIEW_MIN + 0.7],
    ["DROITE", VIEW_MAX - 1.7, 0.7],
    ["ARRIERE", 0, VIEW_MAX - 0.4],
    ["GAUCHE", VIEW_MIN + 1.7, 0.7],
  ];
  for (const [text, x, y] of axisLabels) {
    const t = svgEl("text");
    t.setAttribute("x", String(x));
    t.setAttribute("y", String(y));
    t.setAttribute("class", "scene-axis-label");
    t.textContent = text;
    axisGroup.appendChild(t);
  }
  svg.appendChild(axisGroup);

  // --- Groupes dynamiques (redessines a chaque render) --------------------
  const vectorsGroup = svgEl("g");
  vectorsGroup.setAttribute("class", "scene-vectors");
  svg.appendChild(vectorsGroup);

  const spritesGroup = svgEl("g");
  spritesGroup.setAttribute("class", "scene-sprites");
  svg.appendChild(spritesGroup);

  // --- Joueur : reticule "X" distinct des sources "O" ---------------------
  const playerGroup = svgEl("g");
  playerGroup.setAttribute("class", "scene-player-group");
  playerGroup.setAttribute("tabindex", "0");
  playerGroup.setAttribute("role", "slider");
  playerGroup.setAttribute("aria-label", "Joueur — flèches ou glisser pour déplacer");
  playerGroup.setAttribute("aria-valuetext", "0 m, 0 m");

  const playerRing = svgEl("circle");
  playerRing.setAttribute("r", "0.75");
  playerRing.setAttribute("class", "scene-player-ring");
  const crosshairA = svgEl("line");
  crosshairA.setAttribute("x1", "-0.45"); crosshairA.setAttribute("y1", "-0.45");
  crosshairA.setAttribute("x2", "0.45"); crosshairA.setAttribute("y2", "0.45");
  crosshairA.setAttribute("class", "scene-player-crosshair");
  const crosshairB = svgEl("line");
  crosshairB.setAttribute("x1", "-0.45"); crosshairB.setAttribute("y1", "0.45");
  crosshairB.setAttribute("x2", "0.45"); crosshairB.setAttribute("y2", "-0.45");
  crosshairB.setAttribute("class", "scene-player-crosshair");
  const playerCore = svgEl("circle");
  playerCore.setAttribute("r", "0.12");
  playerCore.setAttribute("class", "scene-player-core");
  playerGroup.appendChild(playerRing);
  playerGroup.appendChild(crosshairA);
  playerGroup.appendChild(crosshairB);
  playerGroup.appendChild(playerCore);
  svg.appendChild(playerGroup);

  // Conteneur "stage" de taille plafonnee (voir .scene-stage, style.css) :
  // le SVG a un viewBox fixe (24x24 m), donc sans plafond il grossit au
  // pixel pres avec .scene-col — sur un grand ecran/fenetre maximisee, tout
  // (police, anneaux, glyphes joueur/sources) devient demesurement gros
  // alors que les proportions restent correctes (signale par l'utilisateur :
  // "tout est trop zoome"). Le stage plafonne la taille visuelle et se
  // centre dans .scene-col au lieu de remplir tout l'espace disponible.
  const stage = document.createElement("div");
  stage.className = "scene-stage";
  stage.appendChild(svg);
  container.appendChild(stage);

  // Legende fixe : resout explicitement "quel symbole est le joueur" sans
  // dependre uniquement de la reconnaissance de forme/couleur.
  const legend = document.createElement("div");
  legend.className = "scene-legend";
  legend.setAttribute("aria-hidden", "true");
  legend.innerHTML = `
    <div class="scene-legend-row">
      <svg class="scene-legend-glyph" viewBox="-1 -1 2 2"><circle r="0.75" class="scene-player-ring"/><line x1="-0.45" y1="-0.45" x2="0.45" y2="0.45" class="scene-player-crosshair"/><line x1="-0.45" y1="0.45" x2="0.45" y2="-0.45" class="scene-player-crosshair"/></svg>
      <span>Joueur (glisser ou flèches)</span>
    </div>
    <div class="scene-legend-row">
      <svg class="scene-legend-glyph" viewBox="-1 -1 2 2"><circle r="0.6" class="scene-sprite-ring"/><circle r="0.14" class="scene-sprite-core"/></svg>
      <span>Source sonore (fixe)</span>
    </div>
  `;
  stage.appendChild(legend);

  let latestPlayer = { x: 0, z: 0 };
  let dragging = false;
  let lastPostAt = 0;

  function movePlayerTo(x: number, z: number, { post }: { post: boolean }) {
    const clampedX = clamp(x, -CLAMP_M, CLAMP_M);
    const clampedZ = clamp(z, -CLAMP_M, CLAMP_M);
    latestPlayer = { x: clampedX, z: clampedZ };
    const { sx, sy } = worldToScreen(clampedX, clampedZ);
    playerGroup.setAttribute("transform", `translate(${sx} ${sy})`);
    playerGroup.setAttribute("aria-valuetext", `${clampedX.toFixed(1)} m, ${clampedZ.toFixed(1)} m`);
    if (post) {
      const now = performance.now();
      if (now - lastPostAt >= DRAG_POST_THROTTLE_MS) {
        lastPostAt = now;
        void postPlayer(clampedX, clampedZ);
      }
    }
  }

  function clientToWorld(clientX: number, clientY: number): { x: number; z: number } {
    const rect = svg.getBoundingClientRect();
    const sx = VIEW_MIN + ((clientX - rect.left) / rect.width) * VIEW_SIZE;
    const sy = VIEW_MIN + ((clientY - rect.top) / rect.height) * VIEW_SIZE;
    return { x: sx, z: sy }; // inverse de worldToScreen : sy = z, donc z = sy
  }

  playerGroup.addEventListener("pointerdown", (ev) => {
    // CRITIQUE : sans preventDefault(), Chromium declenche son propre drag
    // natif (ghost image) sur l'element SVG des le premier mousemove, qui
    // prend le pas sur notre drag pointer-events custom — reproduit et
    // confirme via Playwright (le cercle devient un enorme halo noir/blanc).
    ev.preventDefault();
    // preventDefault() ci-dessus supprime aussi le focus-au-clic par defaut
    // du navigateur (necessaire pour bloquer le drag natif) : sans ce
    // .focus() explicite, cliquer le joueur ne lui donnait jamais le focus,
    // rendant les fleches inutilisables apres un clic (trouve en revue).
    playerGroup.focus();
    dragging = true;
    try {
      playerGroup.setPointerCapture(ev.pointerId);
    } catch {
      // Pointeur deja invalide (ex. sequence d'evenements non standard) —
      // dragging reste true, la souris continue de fonctionner via le
      // pointermove ci-dessous, seule la redirection de capture est perdue.
    }
  });
  playerGroup.addEventListener("pointerup", (ev) => {
    dragging = false;
    try {
      playerGroup.releasePointerCapture(ev.pointerId);
    } catch {
      // deja relachee/invalide — rien a faire.
    }
  });
  // pointercancel (ex. le navigateur interrompt le geste — changement
  // d'onglet, autre pointeur, etc.) n'est PAS garanti d'etre suivi d'un
  // pointerup : sans ce handler, "dragging" restait bloque a true et tout
  // mousemove ulterieur ailleurs sur la page continuait a deplacer le
  // joueur — releve en revue independante round 2.
  playerGroup.addEventListener("pointercancel", (ev) => {
    dragging = false;
    try {
      playerGroup.releasePointerCapture(ev.pointerId);
    } catch {
      // deja relachee/invalide — rien a faire.
    }
  });
  playerGroup.addEventListener("pointermove", (ev) => {
    if (!dragging) return;
    ev.preventDefault();
    const { x, z } = clientToWorld(ev.clientX, ev.clientY);
    movePlayerTo(x, z, { post: true });
  });
  playerGroup.addEventListener("dragstart", (ev) => ev.preventDefault());

  // Deplacement au clavier : le joueur est un role="slider", les fleches
  // doivent le piloter (WAI-ARIA APG) — et c'est thematiquement juste pour
  // un projet d'accessibilite (meme logique que hrtf_profile_listen).
  playerGroup.addEventListener("keydown", (ev) => {
    let dx = 0, dz = 0;
    switch (ev.key) {
      case "ArrowUp": dz = -KEYBOARD_STEP_M; break;
      case "ArrowDown": dz = KEYBOARD_STEP_M; break;
      case "ArrowLeft": dx = -KEYBOARD_STEP_M; break;
      case "ArrowRight": dx = KEYBOARD_STEP_M; break;
      default: return;
    }
    ev.preventDefault();
    movePlayerTo(latestPlayer.x + dx, latestPlayer.z + dz, { post: true });
  });

  function render(state: DemoState) {
    if (!dragging) movePlayerTo(state.player.x, state.player.z, { post: false });

    while (spritesGroup.firstChild) spritesGroup.removeChild(spritesGroup.firstChild);
    while (vectorsGroup.firstChild) vectorsGroup.removeChild(vectorsGroup.firstChild);

    const playerScreen = worldToScreen(latestPlayer.x, latestPlayer.z);

    for (const source of state.sources as SourceState[]) {
      const { sx, sy } = worldToScreen(source.pos.x, source.pos.z);

      const vector = svgEl("line");
      vector.setAttribute("x1", String(playerScreen.sx));
      vector.setAttribute("y1", String(playerScreen.sy));
      vector.setAttribute("x2", String(sx));
      vector.setAttribute("y2", String(sy));
      vector.setAttribute("class", "scene-vector");
      vector.setAttribute("stroke-opacity", String(clamp(0.2 + source.gain * 0.8, 0.2, 1)));
      vectorsGroup.appendChild(vector);

      // Etiquette du vecteur : a une distance fixe (world units) du sprite,
      // jamais au milieu geometrique — sinon a courte distance joueur/source
      // (segment court) elle finit sous l'etiquette du sprite lui-meme.
      // Clampee a 60% du segment cote joueur pour ne jamais depasser sa moitie.
      const dx = playerScreen.sx - sx;
      const dy = playerScreen.sy - sy;
      const segLen = Math.max(Math.hypot(dx, dy), 1e-3);
      const clearance = Math.min(1.3, segLen * 0.6);
      const labelX0 = sx + (dx / segLen) * clearance;
      const labelY = sy + (dy / segLen) * clearance;

      // La propre etiquette de la source (titre + sous-titre format, voir
      // sy-0.85 et sy-0.42 plus bas) occupe un bloc fixe juste au-dessus
      // d'elle. Deux tentatives anterieures (bande verticale fixe, puis
      // decalage proportionnel a "a quel point le vecteur pointe vers le
      // haut") ont chacune laisse passer des cas reels en revue
      // independante round 2 (0.2 m ET 7.2 m de distance, direction pas
      // forcement plein nord). Test AABB direct entre le rectangle du
      // libelle du vecteur et celui de la source — le seul qui couvre
      // vraiment tous les angles/distances, puisqu'il teste le
      // chevauchement reel plutot qu'une approximation de la direction.
      const VECTOR_LABEL_HALF_WIDTH = 1.7; // majore la chaine la plus longue ("12.8 m · -176°")
      const VECTOR_LABEL_HALF_HEIGHT = 0.25;
      const SOURCE_LABEL_HALF_WIDTH = 1.0;
      const SOURCE_LABEL_TOP = sy - 1.2;
      const SOURCE_LABEL_BOTTOM = sy - 0.15;
      const overlapsSourceLabel =
        labelX0 + VECTOR_LABEL_HALF_WIDTH > sx - SOURCE_LABEL_HALF_WIDTH &&
        labelX0 - VECTOR_LABEL_HALF_WIDTH < sx + SOURCE_LABEL_HALF_WIDTH &&
        labelY + VECTOR_LABEL_HALF_HEIGHT > SOURCE_LABEL_TOP &&
        labelY - VECTOR_LABEL_HALF_HEIGHT < SOURCE_LABEL_BOTTOM;
      const sideDir = sx <= 0 ? -1 : 1;
      const labelX = overlapsSourceLabel
        ? sx + sideDir * (SOURCE_LABEL_HALF_WIDTH + VECTOR_LABEL_HALF_WIDTH + 0.15)
        : labelX0;
      vectorsGroup.appendChild(
        labelWithBackground(`${source.distanceM.toFixed(1)} m · ${source.azimuthDeg.toFixed(0)}°`, labelX, labelY, "scene-vector-label"),
      );

      const spriteGroup = svgEl("g");
      spriteGroup.setAttribute("transform", `translate(${sx} ${sy})`);

      const ring = svgEl("circle");
      // rayon de l'anneau module par le gain reel (source.gain) : plus fort
      // = anneau plus large, encodage directement lie a la donnee audio.
      ring.setAttribute("r", String(0.35 + clamp(source.gain, 0, 1) * 0.4));
      ring.setAttribute("class", "scene-sprite-ring");
      const core = svgEl("circle");
      core.setAttribute("r", "0.13");
      core.setAttribute("class", "scene-sprite-core");
      spriteGroup.appendChild(ring);
      spriteGroup.appendChild(core);
      spritesGroup.appendChild(spriteGroup);

      const label = svgEl("text");
      label.setAttribute("x", String(sx));
      label.setAttribute("y", String(sy - 0.85));
      label.setAttribute("class", "scene-sprite-label");
      label.textContent = source.label;
      spritesGroup.appendChild(label);

      const sub = svgEl("text");
      sub.setAttribute("x", String(sx));
      sub.setAttribute("y", String(sy - 0.42));
      sub.setAttribute("class", "scene-sprite-sub");
      sub.textContent = `(${source.format})`;
      spritesGroup.appendChild(sub);
    }

    // PAS de svg.appendChild(playerGroup) ici : playerGroup n'est ajoute
    // qu'une seule fois a l'initialisation (ligne ~154), APRES vectorsGroup/
    // spritesGroup, ce qui suffit a le garder au-dessus (l'ordre DOM ne
    // change jamais, seuls les enfants de vectorsGroup/spritesGroup sont
    // recrees). Re-appeler appendChild ici sur cet element deja connecte,
    // en cours de pointer capture, faisait perdre silencieusement la
    // capture du pointeur ET le focus clavier a chaque tick de polling
    // (~60 ms) — bug trouve par revue independante (Playwright, drag qui se
    // fige apres le premier pointermove ; role="slider" jamais utilisable
    // au clavier en pratique).
  }

  return { render };
}
