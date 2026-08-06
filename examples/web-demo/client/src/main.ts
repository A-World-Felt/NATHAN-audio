import { startPolling } from "./api";

document.querySelector<HTMLDivElement>("#app")!.innerHTML = `
  <p style="padding: 2rem; font-family: ui-monospace, monospace;">
    demo web NATHAN-audio — squelette (Task 7). Suite dans Task 8+.
  </p>
`;

startPolling(
  (state) => console.log("state", state),
  (reachable) => console.log("reachable?", reachable),
);
