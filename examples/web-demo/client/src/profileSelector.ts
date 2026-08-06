import { fetchProfiles, postProfile } from "./api";

export function initProfileSelector(container: HTMLElement): { render(activeId: string): void } {
  container.innerHTML = `
    <section aria-label="Sélecteur de profil HRTF">
      <h2>Catalogue de profils</h2>
      <p class="profile-hint" data-field="hint"></p>
      <ul class="profile-list" data-field="list" role="listbox" aria-label="Profils HRTF disponibles"></ul>
    </section>
  `;

  const list = container.querySelector<HTMLUListElement>('[data-field="list"]')!;
  const hint = container.querySelector<HTMLElement>('[data-field="hint"]')!;
  let activeId = "";

  function paintActive() {
    for (const li of Array.from(list.children)) {
      const el = li as HTMLLIElement;
      el.classList.toggle("profile-active", el.dataset.id === activeId);
      el.setAttribute("aria-selected", String(el.dataset.id === activeId));
    }
  }

  async function onSelect(id: string) {
    hint.textContent = `Bascule vers ${id}…`;
    const result = await postProfile(id);
    if (!result.ok) {
      hint.textContent = `Échec de bascule vers ${id} : ${result.error}`;
      return;
    }
    activeId = id;
    paintActive();
    hint.textContent = `Profil actif : ${id}`;
  }

  fetchProfiles()
    .then(({ profiles, active }) => {
      activeId = active;
      list.innerHTML = profiles
        .map((id) => `<li role="option" data-id="${id}" tabindex="0">${id}</li>`)
        .join("");
      for (const li of Array.from(list.children)) {
        li.addEventListener("click", () => void onSelect((li as HTMLLIElement).dataset.id!));
      }
      paintActive();
      hint.textContent = `Profil actif : ${activeId}`;
    })
    .catch(() => {
      hint.textContent = "Catalogue de profils indisponible — le serveur audio répond-il ?";
    });

  function render(newActiveId: string) {
    if (newActiveId !== activeId) {
      activeId = newActiveId;
      paintActive();
    }
  }

  return { render };
}
