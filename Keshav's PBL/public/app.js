/**
 * ============================================================================
 * PulsePath Emergency Dispatch System v2.0 - Frontend Application Controller
 * Single-Page Application (SPA) logic: Authentication, Dispatch DSA Pipeline,
 * Live Fleet Tracking, Real-Time Emergency Management, SVG Dijkstra Routing Map,
 * Dark/Light Theming, and Historical Auditing.
 * ============================================================================
 */

document.addEventListener("DOMContentLoaded", () => {
    // ------------------------------------------------------------------------
    // 1. DOM Element References
    // ------------------------------------------------------------------------

    // Views
    const loginView = document.querySelector("#login-view");
    const dashboardView = document.querySelector("#dashboard-view");

    // Login Elements
    const loginForm = document.querySelector("#login-form");
    const loginUsernameInput = document.querySelector("#login-username");
    const loginPasswordInput = document.querySelector("#login-password");
    const loginBtn = document.querySelector("#login-btn");
    const loginError = document.querySelector("#login-error");

    // Theme & Navigation Elements
    const themeToggleBtn = document.querySelector("#theme-toggle-btn");
    const themeIcon = document.querySelector("#theme-icon");
    const themeLabel = document.querySelector("#theme-label");
    const logoutBtn = document.querySelector("#logout-btn");
    const navLinks = document.querySelectorAll(".side-nav .nav");

    // 5 Metric Stat Elements
    const statTotal = document.querySelector("#stat-total");
    const statRate = document.querySelector("#stat-rate");
    const statDistance = document.querySelector("#stat-distance");
    const statActive = document.querySelector("#stat-active");
    const statAvailable = document.querySelector("#stat-available");

    // Dispatch Form Elements
    const dispatchForm = document.querySelector("#dispatch-form");
    const dispatchSubmitBtn = document.querySelector("#dispatch-submit-btn");
    const dispatchBtnText = document.querySelector("#dispatch-btn-text");
    const patientNameInput = document.querySelector("#patientName");
    const patientPhoneInput = document.querySelector("#patientPhone");
    const locationSelect = document.querySelector("#location");
    const emergencyInput = document.querySelector("#emergency");
    const severityInputs = document.querySelectorAll('input[name="severity"]');

    // Results Panel Elements
    const emptyState = document.querySelector("#empty-state");
    const resultsContainer = document.querySelector("#results");
    const errorBox = document.querySelector("#error-box");
    const resultTitle = document.querySelector("#result-title");
    const resultSubtitle = document.querySelector("#result-subtitle");
    const outEmergencyId = document.querySelector("#out-emergency-id");
    const outEmergencyIdTag = document.querySelector("#out-emergency-id-tag");
    const outPatient = document.querySelector("#out-patient");
    const outEmergency = document.querySelector("#out-emergency");
    const outPhone = document.querySelector("#out-phone");
    const callPatientBtn = document.querySelector("#call-patient");
    const outAmbulance = document.querySelector("#out-ambulance");
    const outDistance = document.querySelector("#out-distance");
    const outStatus = document.querySelector("#out-status");
    const outHospital = document.querySelector("#out-hospital");
    const outBeds = document.querySelector("#out-beds");
    const candidateList = document.querySelector("#candidate-list");
    const outLogic = document.querySelector("#out-logic");

    // Fleet Tracker Elements
    const fleetGrid = document.querySelector("#fleet-grid");
    const resetFleetBtn = document.querySelector("#reset-fleet-btn");

    // Active Emergencies Elements
    const emergenciesCount = document.querySelector("#active-emergencies-count");
    const emergenciesList = document.querySelector("#emergencies-list");
    const emergenciesEmpty = document.querySelector("#emergencies-empty");

    // Route Network Map Elements
    const mapContainer = document.querySelector("#map-container");

    // History Table Elements
    const historyTable = document.querySelector("#history-table");
    const historyTbody = document.querySelector("#history-tbody");
    const historyEmpty = document.querySelector("#history-empty");
    const refreshHistoryBtn = document.querySelector("#refresh-history-btn");

    // Application State Cache
    let cachedAmbulances = [];
    let autoRefreshTimer = null;

    // ------------------------------------------------------------------------
    // 2. Utility & Sanitization Functions
    // ------------------------------------------------------------------------

    /**
     * Escape special HTML characters to prevent XSS attacks
     * @param {string|number} str - String or value to sanitize
     * @returns {string} Sanitized string
     */
    function escapeHtml(str) {
        if (str === null || str === undefined) return "";
        return String(str)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#039;");
    }

    /**
     * Map location node IDs to human-readable district names
     * @param {number} loc - Location node 0-8
     * @returns {string} Location name
     */
    function getLocationName(loc) {
        switch (Number(loc)) {
            case 0: return "Central HQ";
            case 1: return "North District";
            case 2: return "East District";
            case 3: return "South District";
            case 4: return "West District";
            case 5: return "Zone 5";
            case 6: return "Zone 6 (City Hospital)";
            case 7: return "Zone 7 (General Hospital)";
            case 8: return "Zone 8 (Small Clinic)";
            default: return `Node ${loc}`;
        }
    }

    // ------------------------------------------------------------------------
    // 3. Dark / Light Theme Management
    // ------------------------------------------------------------------------

    /**
     * Apply active theme from localStorage or system preference
     */
    function initTheme() {
        const savedTheme = localStorage.getItem("pulsepath_theme") || "light";
        applyTheme(savedTheme);
    }

    /**
     * Switch theme and persist setting
     * @param {string} theme - 'light' or 'dark'
     */
    function applyTheme(theme) {
        if (theme === "dark") {
            document.body.classList.add("dark-theme");
            if (themeIcon) themeIcon.textContent = "☀️";
            if (themeLabel) themeLabel.textContent = "Light Mode";
        } else {
            document.body.classList.remove("dark-theme");
            if (themeIcon) themeIcon.textContent = "🌙";
            if (themeLabel) themeLabel.textContent = "Dark Mode";
        }
        localStorage.setItem("pulsepath_theme", theme);

        // Re-render map to adopt correct theme colors
        if (cachedAmbulances.length > 0) {
            renderMap(cachedAmbulances);
        }
    }

    if (themeToggleBtn) {
        themeToggleBtn.addEventListener("click", () => {
            const isDark = document.body.classList.contains("dark-theme");
            applyTheme(isDark ? "light" : "dark");
        });
    }

    // ------------------------------------------------------------------------
    // 4. View Management & Authentication State
    // ------------------------------------------------------------------------

    /**
     * Show the login screen and stop auto-refresh polling
     */
    function showLogin() {
        loginView.classList.remove("hidden");
        dashboardView.classList.add("hidden");
        if (loginError) {
            loginError.textContent = "";
            loginError.classList.add("hidden");
        }
        stopAutoRefresh();
    }

    /**
     * Show the main dashboard and initiate all live data listeners
     */
    function showDashboard() {
        loginView.classList.add("hidden");
        dashboardView.classList.remove("hidden");

        // Initial fetch across all modules
        loadAllData();

        // Start 5-second polling interval
        startAutoRefresh();
    }

    /**
     * Load all dashboard modules concurrently
     */
    async function loadAllData() {
        await Promise.allSettled([
            loadStats(),
            loadFleet(),
            loadEmergencies(),
            loadHistory(),
            loadTracking()
        ]);
    }

    /**
     * Check for persisted login session on startup
     */
    function initSession() {
        const savedUser = sessionStorage.getItem("pulsepath_user");
        if (savedUser) {
            showDashboard();
        } else {
            showLogin();
        }
    }

    // ------------------------------------------------------------------------
    // 5. Login & Logout Actions
    // ------------------------------------------------------------------------

    loginForm.addEventListener("submit", async (event) => {
        event.preventDefault();

        const username = loginUsernameInput.value.trim();
        const password = loginPasswordInput.value;

        loginError.textContent = "";
        loginError.classList.add("hidden");

        loginBtn.disabled = true;
        const originalText = loginBtn.firstElementChild.textContent;
        loginBtn.firstElementChild.textContent = "Authenticating…";

        try {
            const response = await fetch("/api/login", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ username, password })
            });

            const data = await response.json();

            if (!response.ok || !data.ok) {
                throw new Error(data.error || "Invalid username or password.");
            }

            sessionStorage.setItem("pulsepath_user", data.username || username);
            loginForm.reset();
            showDashboard();
        } catch (err) {
            loginError.textContent = err.message.includes("Failed to fetch")
                ? "Cannot reach the C++ server. Ensure web_server is running on port 8080."
                : err.message;
            loginError.classList.remove("hidden");
        } finally {
            loginBtn.disabled = false;
            loginBtn.firstElementChild.textContent = originalText;
        }
    });

    logoutBtn.addEventListener("click", () => {
        sessionStorage.removeItem("pulsepath_user");
        showLogin();
    });

    // ------------------------------------------------------------------------
    // 6. Navigation Smooth Scroll & Active Indicator
    // ------------------------------------------------------------------------

    navLinks.forEach((link) => {
        link.addEventListener("click", (event) => {
            event.preventDefault();

            navLinks.forEach((n) => n.classList.remove("active"));
            link.classList.add("active");

            const targetId = link.getAttribute("href");
            const targetEl = document.querySelector(targetId);
            if (targetEl) {
                targetEl.scrollIntoView({ behavior: "smooth", block: "start" });
            }
        });
    });

    // ------------------------------------------------------------------------
    // 7. Severity Radio Button Styling
    // ------------------------------------------------------------------------

    severityInputs.forEach((input) => {
        input.addEventListener("change", () => {
            document.querySelectorAll(".severity-option").forEach((opt) => {
                opt.classList.remove("selected");
            });
            const parentLabel = input.closest(".severity-option");
            if (parentLabel) {
                parentLabel.classList.add("selected");
            }
        });
    });

    // ------------------------------------------------------------------------
    // 8. Dispatch Form Submission & DSA Pipeline Integration
    // ------------------------------------------------------------------------

    dispatchForm.addEventListener("submit", async (event) => {
        event.preventDefault();

        errorBox.textContent = "";
        errorBox.classList.add("hidden");

        dispatchSubmitBtn.disabled = true;
        dispatchBtnText.textContent = "Processing dispatch…";

        const patientName = patientNameInput.value.trim();
        const patientPhone = patientPhoneInput.value.trim();
        const locationVal = parseInt(locationSelect.value, 10);
        const emergency = emergencyInput.value.trim();
        const checkedSeverity = document.querySelector('input[name="severity"]:checked');
        const severityVal = checkedSeverity ? parseInt(checkedSeverity.value, 10) : 4;

        const payload = {
            patientName,
            patientPhone,
            location: locationVal,
            emergency,
            severity: severityVal
        };

        try {
            const response = await fetch("/api/dispatch", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            const data = await response.json();

            if (!response.ok || !data.ok) {
                throw new Error(data.error || "The dispatch request could not be completed.");
            }

            // Display results in the right results panel
            showResults(data, patientPhone);

            // Refresh all live modules
            loadStats();
            loadFleet();
            loadEmergencies();
            loadHistory();
        } catch (err) {
            errorBox.textContent = err.message.includes("Failed to fetch")
                ? "Cannot connect to the C++ backend. Start web_server first, then try again."
                : err.message;
            errorBox.classList.remove("hidden");
        } finally {
            dispatchSubmitBtn.disabled = false;
            dispatchBtnText.textContent = "Dispatch ambulance";
        }
    });

    // ------------------------------------------------------------------------
    // 9. Results Panel Presentation
    // ------------------------------------------------------------------------

    /**
     * Populate the results panel with data returned from /api/dispatch
     * @param {Object} data - Dispatch result object from backend
     * @param {string} phone - Patient phone number entered by user
     */
    function showResults(data, phone) {
        emptyState.classList.add("hidden");
        resultsContainer.classList.remove("hidden");
        errorBox.classList.add("hidden");

        // Banner Status
        resultTitle.textContent = data.assigned
            ? "Ambulance assigned successfully"
            : "Dispatch alert: No ambulance available";

        resultSubtitle.textContent = `Severity: ${data.severityLabel || "Standard"} · Priority Queue processed`;

        if (outEmergencyId) {
            outEmergencyId.textContent = data.emergencyId
                ? `Emergency #${data.emergencyId}`
                : "Emergency Logged";
        }

        if (outEmergencyIdTag) {
            outEmergencyIdTag.textContent = data.emergencyId
                ? `#${data.emergencyId}`
                : "STEP 02–05";
        }

        // Patient Details
        outPatient.textContent = data.patientName || patientNameInput.value.trim() || "Anonymous";
        outEmergency.textContent = `${data.emergency} · ${getLocationName(data.location)}`;

        // Phone call link validation
        const rawPhone = String(phone || "").trim();
        const phoneDigits = rawPhone.replace(/\D/g, "");
        const isValidPhone = /^\+?[0-9]{10,15}$/.test(rawPhone) && phoneDigits.length >= 10;

        if (rawPhone) {
            outPhone.textContent = `Patient phone: ${rawPhone}`;
        } else {
            outPhone.textContent = "No phone number provided";
        }

        if (isValidPhone) {
            callPatientBtn.href = `tel:${rawPhone}`;
            callPatientBtn.classList.remove("hidden");
        } else {
            callPatientBtn.removeAttribute("href");
            callPatientBtn.classList.add("hidden");
        }

        // Assigned Ambulance
        if (data.assigned) {
            outAmbulance.textContent = `Ambulance ${data.ambulanceId}`;
            outDistance.textContent = `${data.ambulanceDistance} km from patient location`;
            outStatus.textContent = "Assigned";
            outStatus.style.background = "var(--green-soft)";
            outStatus.style.color = "var(--green-dark)";
        } else {
            outAmbulance.textContent = "No ambulance assigned";
            outDistance.textContent = data.assignmentMessage || "All units busy or out of range";
            outStatus.textContent = "Pending";
            outStatus.style.background = "var(--yellow-soft)";
            outStatus.style.color = "var(--yellow-dark)";
        }

        // Selected Hospital
        if (data.hospitalName) {
            outHospital.textContent = data.hospitalName;
            outBeds.textContent = `${data.hospitalDistance} km from patient · ${data.availableBeds} available beds`;
        } else {
            outHospital.textContent = "No suitable hospital found";
            outBeds.textContent = data.hospitalMessage || "Bed capacity or facility verification pending";
        }

        // Four Nearest Ambulances Candidates
        candidateList.innerHTML = "";
        const candidates = data.candidates || [];

        if (candidates.length === 0) {
            const emptyCand = document.createElement("div");
            emptyCand.className = "candidate";
            emptyCand.innerHTML = "<span>No nearby candidates found</span>";
            candidateList.appendChild(emptyCand);
        } else {
            candidates.forEach((candidate) => {
                const isSelected = data.assigned && candidate.id === data.ambulanceId;
                const row = document.createElement("div");
                row.className = `candidate${isSelected ? " chosen" : ""}`;

                const label = document.createElement("strong");
                label.textContent = `Ambulance ${candidate.id}${isSelected ? " · Selected" : ""}`;

                const dist = document.createElement("span");
                dist.className = "km";
                dist.textContent = `${candidate.distance} km`;

                row.appendChild(label);
                row.appendChild(dist);
                candidateList.appendChild(row);
            });
        }

        // Logic Summary
        outLogic.textContent = data.logicSummary ||
            "Priority Queue ranked severity; Dijkstra calculated sample-network distances; nearest-four selection filtered unavailable ambulances; DispatchManager applied acceptance and distance-tolerance rules; HospitalSelector checked facilities and available beds.";
    }

    // ------------------------------------------------------------------------
    // 10. System Stats Management (5 Metrics)
    // ------------------------------------------------------------------------

    /**
     * Fetch system metrics from /api/stats and update all 5 stat cards
     */
    async function loadStats() {
        try {
            const response = await fetch("/api/stats");
            if (!response.ok) return;

            const data = await response.json();
            if (data && data.ok) {
                if (statTotal) {
                    statTotal.textContent = data.totalDispatches !== undefined
                        ? String(data.totalDispatches)
                        : "0";
                }
                if (statRate) {
                    statRate.textContent = data.successRate !== undefined
                        ? String(data.successRate)
                        : "0%";
                }
                if (statDistance) {
                    statDistance.textContent = data.avgDistance !== undefined
                        ? `${data.avgDistance} km`
                        : "0.0 km";
                }
                if (statActive) {
                    statActive.textContent = data.activeEmergencies !== undefined
                        ? String(data.activeEmergencies)
                        : "0";
                }
                if (statAvailable) {
                    statAvailable.textContent = data.availableAmbulances !== undefined
                        ? String(data.availableAmbulances)
                        : "0";
                }
            }
        } catch (err) {
            console.warn("Could not retrieve stats from /api/stats:", err);
        }
    }

    // ------------------------------------------------------------------------
    // 11. Fleet Tracker & Ambulance Management
    // ------------------------------------------------------------------------

    /**
     * Retrieve live fleet data from /api/ambulances and render cards & SVG map
     */
    async function loadFleet() {
        try {
            const response = await fetch("/api/ambulances");
            if (!response.ok) return;

            const data = await response.json();
            if (data && data.ok && Array.isArray(data.ambulances)) {
                cachedAmbulances = data.ambulances;
                renderFleetCards(data.ambulances);
                renderMap(data.ambulances);
            }
        } catch (err) {
            console.warn("Could not retrieve fleet data:", err);
        }
    }

    /**
     * Render ambulance cards into #fleet-grid
     * @param {Array} ambulances - List of ambulance objects from backend
     */
    function renderFleetCards(ambulances) {
        if (!fleetGrid) return;
        fleetGrid.innerHTML = "";

        if (ambulances.length === 0) {
            fleetGrid.innerHTML = '<div class="loading-placeholder">No ambulances found in fleet registry.</div>';
            return;
        }

        ambulances.forEach((amb) => {
            const card = document.createElement("div");

            // Status border class & badge styling
            const statusStr = String(amb.status || "Available");
            let borderClass = "status-available";
            let badgeClass = "badge-available";

            if (statusStr === "Assigned") {
                borderClass = "status-assigned";
                badgeClass = "badge-assigned";
            } else if (statusStr === "OnTrip") {
                borderClass = "status-ontrip";
                badgeClass = "badge-ontrip";
            } else if (statusStr === "Unavailable") {
                borderClass = "status-unavailable";
                badgeClass = "badge-unavailable";
            }

            card.className = `ambulance-card ${borderClass}`;

            const regNum = escapeHtml(amb.registration || `AMB-00${amb.id}`);
            const locName = escapeHtml(amb.locationName || getLocationName(amb.location));
            const eta = amb.etaToCentral !== undefined ? `${amb.etaToCentral} km` : "—";
            const driverId = amb.driverId ? `#${amb.driverId}` : "Assigned";

            card.innerHTML = `
                <div class="amb-header">
                    <div class="amb-title-wrap">
                        <span class="amb-badge-icon">🚑</span>
                        <div>
                            <strong>Unit #${escapeHtml(amb.id)}</strong>
                            <div class="amb-reg">${regNum}</div>
                        </div>
                    </div>
                    <span class="badge-status ${badgeClass}">${escapeHtml(statusStr)}</span>
                </div>
                <div class="amb-body">
                    <div class="amb-loc">
                        <span class="amb-loc-icon">📍</span>
                        <span>${locName} (Node ${escapeHtml(amb.location)})</span>
                    </div>
                </div>
                <div class="amb-meta">
                    <span>ETA to HQ: <strong class="amb-eta">${eta}</strong></span>
                    <span>Driver: <strong>${escapeHtml(driverId)}</strong></span>
                </div>
            `;

            fleetGrid.appendChild(card);
        });
    }

    // Reset Fleet Button Handler
    if (resetFleetBtn) {
        resetFleetBtn.addEventListener("click", async () => {
            const confirmed = confirm("Are you sure you want to reset all ambulances and active emergencies to the initial demo state?");
            if (!confirmed) return;

            resetFleetBtn.disabled = true;
            const originalText = resetFleetBtn.innerHTML;
            resetFleetBtn.innerHTML = "<span>↺</span> Resetting…";

            try {
                const response = await fetch("/api/fleet/reset", {
                    method: "POST"
                });
                const data = await response.json();
                if (data && data.ok) {
                    await loadAllData();
                } else {
                    alert(data.error || "Failed to reset fleet.");
                }
            } catch (err) {
                alert("Network error: Could not reset fleet.");
            } finally {
                resetFleetBtn.disabled = false;
                resetFleetBtn.innerHTML = originalText;
            }
        });
    }

    // ------------------------------------------------------------------------
    // 12. Active Emergencies & Status Progression Management
    // ------------------------------------------------------------------------

    /**
     * Retrieve active emergencies from /api/emergencies and render timeline cards
     */
    async function loadEmergencies() {
        try {
            const response = await fetch("/api/emergencies");
            if (!response.ok) return;

            const data = await response.json();
            if (data && data.ok && Array.isArray(data.emergencies)) {
                renderEmergencies(data.emergencies);
            }
        } catch (err) {
            console.warn("Could not retrieve active emergencies:", err);
        }
    }

    /**
     * Render active emergency cards with vertical timeline
     * @param {Array} emergencies - List of emergency objects from backend
     */
    function renderEmergencies(emergencies) {
        if (!emergenciesList || !emergenciesCount) return;

        // Count non-completed emergencies
        const nonCompleted = emergencies.filter(e => e.currentStatus !== "Completed");
        emergenciesCount.textContent = `${nonCompleted.length} Active`;

        if (emergencies.length === 0) {
            emergenciesList.innerHTML = "";
            emergenciesEmpty.classList.remove("hidden");
            return;
        }

        emergenciesEmpty.classList.add("hidden");
        emergenciesList.innerHTML = "";

        // Display newest first
        const sorted = [...emergencies].reverse();

        sorted.forEach((emg) => {
            const card = document.createElement("div");

            // Severity border & badge class
            const sev = String(emg.severity || "").toLowerCase();
            let sevBorder = "severity-border-critical";
            let sevBadge = "badge-critical";

            if (sev.includes("high") || sev === "3") {
                sevBorder = "severity-border-high";
                sevBadge = "badge-high";
            } else if (sev.includes("med") || sev === "2") {
                sevBorder = "severity-border-medium";
                sevBadge = "badge-medium";
            } else if (sev.includes("low") || sev === "1") {
                sevBorder = "severity-border-low";
                sevBadge = "badge-low";
            }

            const isCompleted = emg.currentStatus === "Completed";
            card.className = `emergency-card ${sevBorder}${isCompleted ? " completed" : ""}`;

            // Status badge class
            let statusBadgeClass = "badge-assigned";
            if (isCompleted) statusBadgeClass = "status-completed";
            else if (emg.currentStatus === "En-Route") statusBadgeClass = "badge-ontrip";
            else if (emg.currentStatus === "Arrived") statusBadgeClass = "badge-available";
            else if (emg.currentStatus === "Pending") statusBadgeClass = "status-pending";

            // Next button text
            let nextActionLabel = "Advance Status →";
            if (emg.currentStatus === "Dispatched") nextActionLabel = "Mark En-Route →";
            else if (emg.currentStatus === "En-Route") nextActionLabel = "Mark Arrived at Patient →";
            else if (emg.currentStatus === "Arrived") nextActionLabel = "Complete & Release Ambulance ✓";

            // Timeline markup
            let timelineHtml = "";
            if (Array.isArray(emg.timeline) && emg.timeline.length > 0) {
                timelineHtml = emg.timeline.map((evt, idx) => {
                    const isLast = idx === emg.timeline.length - 1;
                    return `
                        <div class="timeline-step${isLast ? " completed-step" : ""}">
                            <span class="timeline-dot"></span>
                            <div class="timeline-step-header">
                                <span class="timeline-status-name">${escapeHtml(evt.status)}</span>
                                <span class="timeline-time">${escapeHtml(evt.timestamp || "")}</span>
                            </div>
                            <div class="timeline-msg">${escapeHtml(evt.message || "")}</div>
                        </div>
                    `;
                }).join("");
            } else {
                timelineHtml = '<div class="timeline-msg">No timeline events recorded.</div>';
            }

            const patientName = escapeHtml(emg.patientName || "Anonymous Patient");
            const patientPhone = emg.phone ? `<span class="patient-contact">· 📞 ${escapeHtml(emg.phone)}</span>` : "";
            const locName = escapeHtml(emg.locationName || getLocationName(emg.location));
            const ambInfo = emg.ambulanceId && emg.ambulanceId > 0
                ? `🚑 Amb ${escapeHtml(emg.ambulanceId)} (${escapeHtml(emg.ambulanceDistance)} km)`
                : "🚑 No ambulance assigned";
            const hospInfo = emg.hospitalName
                ? `🏥 ${escapeHtml(emg.hospitalName)} (${escapeHtml(emg.hospitalDistance)} km)`
                : "🏥 No hospital selected";

            card.innerHTML = `
                <div class="emergency-card-header">
                    <div>
                        <div class="emergency-card-title">
                            <span class="emergency-id-tag">Emergency #${escapeHtml(emg.id)}</span>
                            <span class="severity-badge ${sevBadge}">${escapeHtml(emg.severity || "Standard")}</span>
                            <span class="status-badge ${statusBadgeClass}">${escapeHtml(emg.currentStatus)}</span>
                        </div>
                        <div class="patient-headline">
                            ${patientName} ${patientPhone}
                        </div>
                        <p class="emergency-desc-text">${escapeHtml(emg.emergency)}</p>
                    </div>
                </div>

                <div class="emergency-resources">
                    <span class="resource-pill">📍 Location: <strong>${locName} (Node ${escapeHtml(emg.location)})</strong></span>
                    <span class="resource-pill">${ambInfo}</span>
                    <span class="resource-pill">${hospInfo}</span>
                </div>

                <div class="timeline-wrapper">
                    <div class="timeline-title">STATUS TIMELINE</div>
                    <div class="timeline">
                        ${timelineHtml}
                    </div>
                </div>

                <div class="emergency-card-actions">
                    ${isCompleted
                        ? '<span class="status-badge status-completed">✓ Emergency Completed &amp; Archived</span>'
                        : emg.currentStatus === "Pending"
                            ? '<span class="status-badge status-pending">Pending Ambulance Assignment</span>'
                            : `<button type="button" class="advance-btn" data-emergency-id="${escapeHtml(emg.id)}">
                                    <span>${nextActionLabel}</span>
                               </button>`
                    }
                </div>
            `;

            // Attach event listener to advance status button
            const advanceBtn = card.querySelector(".advance-btn");
            if (advanceBtn) {
                advanceBtn.addEventListener("click", () => {
                    advanceEmergencyStatus(emg.id, advanceBtn);
                });
            }

            emergenciesList.appendChild(card);
        });
    }

    /**
     * Advance the lifecycle status of an emergency (Dispatched -> En-Route -> Arrived -> Completed)
     * @param {number} emergencyId - ID of emergency to advance
     * @param {HTMLButtonElement} btn - The clicked button element
     */
    async function advanceEmergencyStatus(emergencyId, btn) {
        if (!emergencyId) return;

        btn.disabled = true;
        const originalText = btn.innerHTML;
        btn.innerHTML = "<span>Updating…</span>";

        try {
            const response = await fetch("/api/emergency/update", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ emergencyId: Number(emergencyId) })
            });

            const data = await response.json();

            if (!response.ok || !data.ok) {
                throw new Error(data.error || "Could not advance emergency status.");
            }

            // Immediately refresh all impacted state
            await Promise.all([
                loadEmergencies(),
                loadFleet(),
                loadStats(),
                loadHistory()
            ]);
        } catch (err) {
            alert(err.message || "Failed to advance emergency status.");
            btn.disabled = false;
            btn.innerHTML = originalText;
        }
    }

    // ------------------------------------------------------------------------
    // 13. Route Network Map Rendering (9-Node Dijkstra Graph SVG)
    // ------------------------------------------------------------------------

    /**
     * Render the 9-node Dijkstra routing network map with live ambulance positions
     * @param {Array} ambulances - Array of ambulance objects with location properties
     */
    function renderMap(ambulances = []) {
        if (!mapContainer) return;

        const isDark = document.body.classList.contains("dark-theme");

        // Color palettes for Light and Dark modes
        const colors = isDark ? {
            edge: "#334155",
            edgeLabelBg: "#1e293b",
            edgeLabelStroke: "#475569",
            edgeText: "#cbd5e1",
            centralFill: "#2563eb",
            centralStroke: "#3b82f6",
            districtFill: "#15803d",
            districtStroke: "#22c55e",
            zone5Fill: "#0f766e",
            zone5Stroke: "#14b8a6",
            hospitalFill: "#c2410c",
            hospitalStroke: "#f97316",
            textMain: "#f1f5f9",
            textSub: "#94a3b8",
            ambBadgeBg: "#1e293b",
            ambBadgeBorder: "#3b82f6"
        } : {
            edge: "#cbd5e1",
            edgeLabelBg: "#ffffff",
            edgeLabelStroke: "#cbd5e1",
            edgeText: "#475569",
            centralFill: "#1d4ed8",
            centralStroke: "#3b82f6",
            districtFill: "#15803d",
            districtStroke: "#22c55e",
            zone5Fill: "#0d9488",
            zone5Stroke: "#14b8a6",
            hospitalFill: "#ea580c",
            hospitalStroke: "#fb923c",
            textMain: "#1f2937",
            textSub: "#6b7280",
            ambBadgeBg: "#ffffff",
            ambBadgeBorder: "#cbd5e1"
        };

        // Graph node definitions (9-node Dijkstra layout inside 500x500 viewport)
        const nodes = [
            { id: 0, x: 250, y: 250, r: 28, name: "Central HQ", type: "central", fill: colors.centralFill, stroke: colors.centralStroke },
            { id: 1, x: 250, y: 65,  r: 22, name: "North District", type: "district", fill: colors.districtFill, stroke: colors.districtStroke },
            { id: 2, x: 430, y: 190, r: 22, name: "East District", type: "district", fill: colors.districtFill, stroke: colors.districtStroke },
            { id: 3, x: 250, y: 435, r: 22, name: "South District", type: "district", fill: colors.districtFill, stroke: colors.districtStroke },
            { id: 4, x: 70,  y: 190, r: 22, name: "West District", type: "district", fill: colors.districtFill, stroke: colors.districtStroke },
            { id: 5, x: 385, y: 95,  r: 22, name: "Zone 5", type: "zone", fill: colors.zone5Fill, stroke: colors.zone5Stroke },
            { id: 6, x: 430, y: 365, r: 24, name: "City Hospital", type: "hospital", fill: colors.hospitalFill, stroke: colors.hospitalStroke },
            { id: 7, x: 70,  y: 95,  r: 24, name: "General Hosp", type: "hospital", fill: colors.hospitalFill, stroke: colors.hospitalStroke },
            { id: 8, x: 70,  y: 365, r: 24, name: "Small Clinic", type: "hospital", fill: colors.hospitalFill, stroke: colors.hospitalStroke }
        ];

        // Edges connecting Node 0 to all other nodes with weights
        const edges = [
            { from: 0, to: 1, dist: "4 km" },
            { from: 0, to: 2, dist: "5 km" },
            { from: 0, to: 3, dist: "8 km" },
            { from: 0, to: 4, dist: "10 km" },
            { from: 0, to: 5, dist: "2 km" },
            { from: 0, to: 6, dist: "3 km" },
            { from: 0, to: 7, dist: "5 km" },
            { from: 0, to: 8, dist: "7 km" }
        ];

        // Map ambulances to locations
        const ambByLocation = {};
        ambulances.forEach((amb) => {
            const loc = Number(amb.location);
            if (!ambByLocation[loc]) ambByLocation[loc] = [];
            ambByLocation[loc].push(amb);
        });

        // Generate Edge SVG
        let edgesSvg = "";
        edges.forEach((edge) => {
            const n1 = nodes[edge.from];
            const n2 = nodes[edge.to];
            const midX = (n1.x + n2.x) / 2;
            const midY = (n1.y + n2.y) / 2;

            edgesSvg += `
                <g class="graph-edge-group">
                    <line x1="${n1.x}" y1="${n1.y}" x2="${n2.x}" y2="${n2.y}"
                          stroke="${colors.edge}" stroke-width="2.5" stroke-dasharray="5 3" />
                    <!-- Edge distance pill -->
                    <rect x="${midX - 22}" y="${midY - 11}" width="44" height="22" rx="6"
                          fill="${colors.edgeLabelBg}" stroke="${colors.edgeLabelStroke}" stroke-width="1.2" />
                    <text x="${midX}" y="${midY + 4}" fill="${colors.edgeText}"
                          font-size="10" font-weight="700" text-anchor="middle">${edge.dist}</text>
                </g>
            `;
        });

        // Generate Node SVG & Markers
        let nodesSvg = "";
        nodes.forEach((node) => {
            const isCenter = node.id === 0;
            const isHospital = node.type === "hospital";
            const ambsAtNode = ambByLocation[node.id] || [];

            // Icon for hospitals
            const hospitalIcon = isHospital
                ? `<text x="${node.x}" y="${node.y + 6}" font-size="16" text-anchor="middle">🏥</text>`
                : "";

            // Center HQ icon
            const centerIcon = isCenter
                ? `<text x="${node.x}" y="${node.y + 6}" font-size="16" text-anchor="middle">🏢</text>`
                : "";

            // District number
            const nodeNumText = (!isHospital && !isCenter)
                ? `<text x="${node.x}" y="${node.y + 5}" fill="#ffffff" font-size="13" font-weight="800" text-anchor="middle">${node.id}</text>`
                : "";

            // Label positioning
            let labelY = node.y + node.r + 15;
            if (node.id === 1) labelY = node.y - node.r - 8; // Top node label goes above

            // Ambulance markers pill near node
            let ambBadgeSvg = "";
            if (ambsAtNode.length > 0) {
                const ambText = ambsAtNode.map(a => `#${a.id}`).join(",");
                const badgeWidth = Math.max(54, ambsAtNode.length * 28 + 26);
                const badgeX = node.x - badgeWidth / 2;
                const badgeY = isCenter ? node.y - node.r - 26 : node.y + node.r + 26;

                ambBadgeSvg = `
                    <g class="amb-marker-group" filter="drop-shadow(0 2px 4px rgba(0,0,0,0.18))">
                        <rect x="${badgeX}" y="${badgeY}" width="${badgeWidth}" height="20" rx="10"
                              fill="${colors.ambBadgeBg}" stroke="${colors.ambBadgeBorder}" stroke-width="1.5" />
                        <text x="${node.x}" y="${badgeY + 14}" font-size="11" font-weight="700"
                              fill="${colors.textMain}" text-anchor="middle">🚑 ${escapeHtml(ambText)}</text>
                    </g>
                `;
            }

            nodesSvg += `
                <g class="graph-node-group" id="map-node-${node.id}">
                    <!-- Outer pulse for central HQ -->
                    ${isCenter ? `<circle cx="${node.x}" cy="${node.y}" r="${node.r + 7}" fill="none" stroke="${colors.centralStroke}" stroke-width="1.5" opacity="0.4" stroke-dasharray="4 2" />` : ""}

                    <!-- Main node circle -->
                    <circle cx="${node.x}" cy="${node.y}" r="${node.r}"
                            fill="${node.fill}" stroke="${node.stroke}" stroke-width="3"
                            filter="drop-shadow(0 4px 6px rgba(0,0,0,0.15))" />

                    ${centerIcon}
                    ${hospitalIcon}
                    ${nodeNumText}

                    <!-- Node name label -->
                    <text x="${node.x}" y="${labelY}" fill="${colors.textMain}"
                          font-size="11" font-weight="700" text-anchor="middle">${escapeHtml(node.name)}</text>

                    <!-- Ambulance badge -->
                    ${ambBadgeSvg}
                </g>
            `;
        });

        // Assemble full SVG
        mapContainer.innerHTML = `
            <svg viewBox="0 0 500 500" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="Dijkstra Road Network Map">
                <defs>
                    <filter id="mapShadow" x="-10%" y="-10%" width="120%" height="120%">
                        <feDropShadow dx="0" dy="2" stdDeviation="3" flood-opacity="0.1" />
                    </filter>
                </defs>
                <!-- Edges Layer -->
                <g id="map-edges-layer">
                    ${edgesSvg}
                </g>
                <!-- Nodes Layer -->
                <g id="map-nodes-layer">
                    ${nodesSvg}
                </g>
            </svg>
        `;
    }

    // ------------------------------------------------------------------------
    // 14. Dispatch Audit History Management
    // ------------------------------------------------------------------------

    /**
     * Retrieve audit history from /api/history and render the history table
     */
    async function loadHistory() {
        try {
            const response = await fetch("/api/history");
            if (!response.ok) {
                throw new Error(`History returned status ${response.status}`);
            }

            const data = await response.json();

            if (data && data.ok && Array.isArray(data.records) && data.records.length > 0) {
                historyTbody.innerHTML = "";
                historyEmpty.classList.add("hidden");
                historyTable.classList.remove("hidden");

                data.records.forEach((rec) => {
                    const row = document.createElement("tr");

                    // Severity badge class
                    const sevStr = String(rec.severity || "").toLowerCase();
                    let sevClass = "badge-low";
                    if (sevStr.includes("crit") || sevStr === "4") {
                        sevClass = "badge-critical";
                    } else if (sevStr.includes("high") || sevStr === "3") {
                        sevClass = "badge-high";
                    } else if (sevStr.includes("med") || sevStr === "2") {
                        sevClass = "badge-medium";
                    }

                    // Status badge class
                    const statusStr = String(rec.status || "Pending");
                    const statusLower = statusStr.toLowerCase();
                    let statusClass = "status-assigned";
                    if (statusLower.includes("fail") || statusLower.includes("reject")) {
                        statusClass = "status-failed";
                    } else if (statusLower.includes("pend")) {
                        statusClass = "status-pending";
                    } else if (statusLower.includes("comp")) {
                        statusClass = "status-completed";
                    }

                    const patientName = escapeHtml(rec.patientName || "Unknown");
                    const phone = rec.phone ? `<small class="patient-phone">${escapeHtml(rec.phone)}</small>` : "";
                    const emergency = escapeHtml(rec.emergency || "Emergency");
                    const loc = rec.location !== undefined ? `Node ${escapeHtml(rec.location)} (${getLocationName(rec.location)})` : "—";
                    const ambText = rec.ambulanceId && rec.ambulanceId > 0
                        ? `🚑 Amb ${escapeHtml(rec.ambulanceId)} (${escapeHtml(rec.ambulanceDistance)} km)`
                        : "—";
                    const hospText = rec.hospitalName
                        ? `🏥 ${escapeHtml(rec.hospitalName)} (${escapeHtml(rec.hospitalDistance)} km)`
                        : "—";

                    row.innerHTML = `
                        <td><strong>#${escapeHtml(rec.id)}</strong></td>
                        <td class="history-time">${escapeHtml(rec.timestamp || "—")}</td>
                        <td>
                            <div class="patient-cell">
                                <span class="patient-name">${patientName}</span>
                                ${phone}
                            </div>
                        </td>
                        <td>
                            <span class="emergency-desc">${emergency}</span>
                            <span class="location-tag">${loc}</span>
                        </td>
                        <td><span class="severity-badge ${sevClass}">${escapeHtml(rec.severity || "—")}</span></td>
                        <td>${ambText}</td>
                        <td>${hospText}</td>
                        <td><span class="status-badge ${statusClass}">${escapeHtml(statusStr)}</span></td>
                    `;

                    historyTbody.appendChild(row);
                });
            } else {
                historyTbody.innerHTML = "";
                historyEmpty.classList.remove("hidden");
            }
        } catch (err) {
            console.warn("Could not retrieve dispatch history:", err);
            historyTbody.innerHTML = "";
            historyEmpty.classList.remove("hidden");
        }
    }

    if (refreshHistoryBtn) {
        refreshHistoryBtn.addEventListener("click", () => {
            loadHistory();
            loadStats();
        });
    }

    // ------------------------------------------------------------------------
    // 15. Periodic Auto-Refresh Routine (Every 5 seconds)
    // ------------------------------------------------------------------------

    function startAutoRefresh() {
        stopAutoRefresh();
        autoRefreshTimer = setInterval(() => {
            // Only poll if the dashboard is actively visible
            if (!dashboardView.classList.contains("hidden")) {
                loadFleet();
                loadEmergencies();
                loadStats();
                loadTracking();
            }
        }, 5000);
    }

    function stopAutoRefresh() {
        if (autoRefreshTimer) {
            clearInterval(autoRefreshTimer);
            autoRefreshTimer = null;
        }
    }

    // ------------------------------------------------------------------------
    // 17. Live Ambulance Tracking
    // ------------------------------------------------------------------------

    const trackingCardsContainer = document.querySelector("#tracking-cards");
    const trackingEmpty = document.querySelector("#tracking-empty");
    let trackingTimer = null;

    /**
     * Format seconds into MM:SS display
     */
    function formatETA(seconds) {
        if (seconds <= 0) return "0:00";
        const m = Math.floor(seconds / 60);
        const s = Math.floor(seconds % 60);
        return `${m}:${s.toString().padStart(2, "0")}`;
    }

    /**
     * Fetch live tracking data from /api/tracking and render cards
     */
    async function loadTracking() {
        try {
            const response = await fetch("/api/tracking");
            if (!response.ok) return;
            const data = await response.json();

            if (!data.ok || !Array.isArray(data.tracking) || data.tracking.length === 0) {
                if (trackingCardsContainer) {
                    trackingCardsContainer.innerHTML = "";
                    trackingCardsContainer.appendChild(trackingEmpty || createTrackingEmpty());
                    if (trackingEmpty) trackingEmpty.style.display = "";
                }
                return;
            }

            if (trackingEmpty) trackingEmpty.style.display = "none";
            trackingCardsContainer.innerHTML = "";

            data.tracking.forEach((t) => {
                const phaseClass = t.currentPhase === "at_patient" ? "phase-at_patient"
                                 : t.currentPhase === "to_hospital" ? "phase-to_hospital"
                                 : "";
                const phaseTagClass = t.currentPhase === "at_patient" ? "at-patient"
                                   : t.currentPhase === "to_hospital" ? "to-hospital"
                                   : "";

                // Severity badge class
                const sevLower = (t.severity || "").toLowerCase();
                let sevClass = "badge-low";
                if (sevLower.includes("crit")) sevClass = "badge-critical";
                else if (sevLower.includes("high")) sevClass = "badge-high";
                else if (sevLower.includes("med")) sevClass = "badge-medium";

                const card = document.createElement("div");
                card.className = `tracking-card ${phaseClass}`;

                const progress = Math.min(100, Math.max(0, t.progress || 0));
                const distRem = parseFloat(t.distanceRemaining || 0).toFixed(1);
                const eta = formatETA(t.etaSeconds || 0);

                card.innerHTML = `
                    <div class="tracking-header">
                        <h3>
                            🚑 Ambulance #${escapeHtml(t.ambulanceId)} 
                            <small style="color:var(--muted);font-weight:400">(${escapeHtml(t.registration)})</small>
                        </h3>
                        <span class="tracking-phase ${phaseTagClass}">
                            ${t.currentPhase === "to_patient" ? "🔴" : t.currentPhase === "at_patient" ? "🟠" : "🔵"}
                            ${escapeHtml(t.phaseLabel)}
                        </span>
                    </div>

                    <div style="margin-bottom:10px;font-size:0.9rem;color:var(--muted)">
                        Emergency #${t.emergencyId} · <span class="severity-badge ${sevClass}">${escapeHtml(t.severity)}</span>
                        · ${escapeHtml(t.patientName)} — ${escapeHtml(t.emergency)}
                    </div>

                    <div class="tracking-progress-wrap">
                        <div class="tracking-progress-bar">
                            <div class="tracking-progress-fill" style="width:${progress}%"></div>
                        </div>
                        <div class="tracking-progress-label">
                            <span>${progress.toFixed(1)}% complete</span>
                            <span>${distRem} km remaining</span>
                        </div>
                    </div>

                    <div class="tracking-stats">
                        <div class="tracking-stat">
                            <div class="stat-icon">⏱️</div>
                            <span class="stat-value eta-value">${eta}</span>
                            <span class="stat-label">ETA</span>
                        </div>
                        <div class="tracking-stat">
                            <div class="stat-icon">📏</div>
                            <span class="stat-value">${distRem} km</span>
                            <span class="stat-label">Distance Left</span>
                        </div>
                        <div class="tracking-stat">
                            <div class="stat-icon">🏎️</div>
                            <span class="stat-value">${t.speedKmH}</span>
                            <span class="stat-label">km/h (sim)</span>
                        </div>
                        <div class="tracking-stat">
                            <div class="stat-icon">📍</div>
                            <span class="stat-value">${t.distanceTotal} km</span>
                            <span class="stat-label">Total Distance</span>
                        </div>
                    </div>

                    <div class="tracking-route">
                        <strong>Route:</strong> 
                        ${t.currentPhase === "to_patient" || t.currentPhase === "at_patient" 
                            ? `Node ${t.startLocation} → Patient (Node ${t.patientLocation})` 
                            : `Patient (Node ${t.patientLocation}) → ${escapeHtml(t.hospitalName)} (Node ${t.hospitalLocation})`}
                        &nbsp;|&nbsp; 
                        <strong>Hospital:</strong> ${escapeHtml(t.hospitalName)}
                    </div>
                `;
                trackingCardsContainer.appendChild(card);
            });

            // Also refresh fleet + emergencies when tracking updates
        } catch (err) {
            console.warn("Could not load tracking data:", err);
        }
    }

    function createTrackingEmpty() {
        const div = document.createElement("div");
        div.className = "tracking-empty";
        div.innerHTML = `
            <div class="empty-icon">📍</div>
            <p>No ambulances currently in transit.</p>
            <small>Dispatch an emergency to see live tracking with distance, ETA, and position.</small>
        `;
        return div;
    }

    // Start a fast 2-second refresh for tracking data
    function startTrackingRefresh() {
        stopTrackingRefresh();
        trackingTimer = setInterval(() => {
            if (!dashboardView.classList.contains("hidden")) {
                loadTracking();
            }
        }, 2000);
    }

    function stopTrackingRefresh() {
        if (trackingTimer) {
            clearInterval(trackingTimer);
            trackingTimer = null;
        }
    }

    // ------------------------------------------------------------------------
    // 18. Application Initialization
    // ------------------------------------------------------------------------
    initTheme();
    initSession();

    // Start tracking refresh if dashboard is visible
    if (!dashboardView.classList.contains("hidden")) {
        loadTracking();
        startTrackingRefresh();
    }
});
