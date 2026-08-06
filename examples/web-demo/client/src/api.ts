export interface SourceState {
  id: string;
  label: string;
  asset: string;
  format: "mp3" | "wav";
  pos: { x: number; z: number };
  distanceM: number;
  azimuthDeg: number;
  gain: number;
}

export interface DemoState {
  player: { x: number; z: number };
  profile: { active: string; default: string; usedFallback: boolean; fallbackReason: string };
  hrtf: { status: string; renderer: string; version: string; vendor: string; sampleRateHz: number };
  sources: SourceState[];
  metrics: {
    framesProcessed: number;
    frameTimeUs: { last: number; mean: number; min: number; max: number; p95: number };
    frameBudgetMs: number;
    fpsReal: number;
    cpuBudgetPercent: number;
    sourcesSimultaneous: number;
    meetsRAud03: boolean;
  };
  mp3: {
    path: string;
    fileSizeBytes: number;
    sampleRateHz: number;
    channels: number;
    durationSec: number;
    totalSamples: number;
  };
}

export interface ProfilesResponse {
  profiles: string[];
  active: string;
}

export interface ProfileSwitchError {
  ok: false;
  error: string;
}

export interface ProfileSwitchOk {
  ok: true;
  state: DemoState;
}

const SERVER_URL = import.meta.env.VITE_SERVER_URL ?? "http://127.0.0.1:8787";

export async function fetchState(): Promise<DemoState> {
  const res = await fetch(`${SERVER_URL}/api/state`);
  if (!res.ok) throw new Error(`GET /api/state -> ${res.status}`);
  return (await res.json()) as DemoState;
}

export async function fetchProfiles(): Promise<ProfilesResponse> {
  const res = await fetch(`${SERVER_URL}/api/profiles`);
  if (!res.ok) throw new Error(`GET /api/profiles -> ${res.status}`);
  return (await res.json()) as ProfilesResponse;
}

export async function postPlayer(x: number, z: number): Promise<void> {
  await fetch(`${SERVER_URL}/api/player`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ x, z }),
  });
  // Fire-and-forget par conception (cf. spec) : on ignore la reponse, la
  // prochaine boucle de polling /api/state refletera la nouvelle position.
}

export async function postProfile(id: string): Promise<ProfileSwitchOk | ProfileSwitchError> {
  const res = await fetch(`${SERVER_URL}/api/profile`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id }),
  });
  const body = await res.json();
  if (!res.ok) return { ok: false, error: (body as { error: string }).error };
  return { ok: true, state: body as DemoState };
}

// Interroge /api/state en boucle. onState recoit chaque snapshot reussi ;
// onReachability(false) est appele des la premiere erreur reseau (serveur
// injoignable), onReachability(true) des que ca redevient joignable.
// Retourne une fonction d'arret.
export function startPolling(
  onState: (state: DemoState) => void,
  onReachability: (reachable: boolean) => void,
  intervalMs = 60,
): () => void {
  let stopped = false;
  let wasReachable = true;

  async function tick() {
    if (stopped) return;
    try {
      const state = await fetchState();
      if (!wasReachable) {
        wasReachable = true;
        onReachability(true);
      }
      onState(state);
    } catch {
      if (wasReachable) {
        wasReachable = false;
        onReachability(false);
      }
    } finally {
      if (!stopped) setTimeout(tick, intervalMs);
    }
  }

  tick();
  return () => {
    stopped = true;
  };
}
