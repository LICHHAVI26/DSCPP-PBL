const form = document.querySelector("#dispatch-form");
const submitButton = form.querySelector("button[type=submit]");
const emptyState = document.querySelector("#empty-state");
const results = document.querySelector("#results");
const errorBox = document.querySelector("#error-box");

document.querySelectorAll('input[name="severity"]').forEach((input) => {
  input.addEventListener("change", () => {
    document.querySelectorAll(".severity-option").forEach((option) => option.classList.remove("selected"));
    input.closest(".severity-option").classList.add("selected");
  });
});

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  errorBox.classList.add("hidden");
  submitButton.disabled = true;
  submitButton.querySelector("span:first-child").textContent = "Processing request…";

  const data = new FormData(form);
  const payload = {
    patientName: String(data.get("patientName") || "").trim(),
    location: Number(data.get("location")),
    emergency: String(data.get("emergency") || "").trim(),
    severity: Number(data.get("severity"))
  };

  try {
    const response = await fetch("/api/dispatch", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const body = await response.json();
    if (!response.ok || !body.ok) throw new Error(body.error || "The dispatch request could not be completed.");
    showResults(body);
  } catch (error) {
    errorBox.textContent = error.message.includes("Failed to fetch")
      ? "Cannot connect to the C++ backend. Start web_server first, then refresh this page."
      : error.message;
    errorBox.classList.remove("hidden");
  } finally {
    submitButton.disabled = false;
    submitButton.querySelector("span:first-child").textContent = "Dispatch ambulance";
  }
});

function showResults(data) {
  emptyState.classList.add("hidden");
  results.classList.remove("hidden");
  errorBox.classList.add("hidden");
  document.querySelector("#result-title").textContent = data.assigned ? "Ambulance assigned successfully" : "Dispatch needs attention";
  document.querySelector("#result-subtitle").textContent = `Severity: ${data.severityLabel} · Priority Queue processed`;
  document.querySelector("#out-patient").textContent = data.patientName;
  document.querySelector("#out-emergency").textContent = `${data.emergency} · Location ${data.location}`;
  document.querySelector("#out-ambulance").textContent = data.assigned ? `Ambulance ${data.ambulanceId}` : "No ambulance assigned";
  document.querySelector("#out-distance").textContent = data.assigned ? `${data.ambulanceDistance} km from patient` : data.assignmentMessage;
  document.querySelector("#out-status").textContent = data.assigned ? "Assigned" : "Pending";
  document.querySelector("#out-hospital").textContent = data.hospitalName || "No suitable hospital";
  document.querySelector("#out-beds").textContent = data.hospitalName ? `${data.hospitalDistance} km · ${data.availableBeds} available beds` : (data.hospitalMessage || "");
  document.querySelector("#out-logic").textContent = data.logicSummary;

  const list = document.querySelector("#candidate-list");
  list.replaceChildren();
  data.candidates.forEach((candidate) => {
    const row = document.createElement("div");
    row.className = "candidate" + (data.assigned && candidate.id === data.ambulanceId ? " chosen" : "");
    const left = document.createElement("strong");
    left.textContent = `Ambulance ${candidate.id}${data.assigned && candidate.id === data.ambulanceId ? " · Selected" : ""}`;
    const right = document.createElement("span");
    right.className = "km";
    right.textContent = `${candidate.distance} km`;
    row.append(left, right);
    list.append(row);
  });
}
