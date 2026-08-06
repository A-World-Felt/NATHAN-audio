import { fetchProfiles, postProfile } from "./api";

export function initProfileSelector(container: HTMLElement): { render(activeId: string): void } {
  container.innerHTML = `
    <label class="sr-only" for="profileSearch">Filtrer les profils</label>
    <input class="profile-search" id="profileSearch" type="search"
           placeholder="Filtrer (ex. 003, subject_1…)" autocomplete="off" />
    <label class="sr-only" for="profileSelect">Profil HRTF actif</label>
    <select class="profile-select" id="profileSelect" data-field="select" aria-label="Profils HRTF disponibles"></select>
    <p class="profile-hint" data-field="hint"></p>
  `;

  const select = container.querySelector<HTMLSelectElement>('[data-field="select"]')!;
  const hint = container.querySelector<HTMLElement>('[data-field="hint"]')!;
  const search = container.querySelector<HTMLInputElement>("#profileSearch")!;
  let activeId = "";

  function applyFilter() {
    const q = search.value.trim().toLowerCase();
    for (const opt of Array.from(select.options)) {
      opt.hidden = q !== "" && !opt.value.toLowerCase().includes(q);
    }
    // Si l'option selectionnee vient d'etre masquee par le filtre, le
    // <select> natif garde quand meme sa valeur affichee — inoffensif,
    // mais on evite de la re-proposer comme choix tant que le filtre est actif.
  }
  search.addEventListener("input", applyFilter);

  async function onSelect(id: string) {
    hint.textContent = `Bascule vers ${id}…`;
    const result = await postProfile(id);
    if (!result.ok) {
      hint.textContent = `Échec de bascule vers ${id} : ${result.error}`;
      select.value = activeId; // revert l'affichage au profil reellement actif
      return;
    }
    activeId = id;
    hint.textContent = `Profil actif : ${id}`;
  }

  select.addEventListener("change", () => void onSelect(select.value));

  fetchProfiles()
    .then(({ profiles, active }) => {
      activeId = active;
      select.innerHTML = profiles.map((id) => `<option value="${id}">${id}</option>`).join("");
      select.value = activeId;
      hint.textContent = `Profil actif : ${activeId}`;
    })
    .catch(() => {
      hint.textContent = "Catalogue de profils indisponible — le serveur audio répond-il ?";
    });

  function render(newActiveId: string) {
    if (newActiveId !== activeId) {
      activeId = newActiveId;
      select.value = activeId;
    }
  }

  return { render };
}
