const dom = {
  connectButton: document.getElementById('connectButton'),
  disconnectButton: document.getElementById('disconnectButton'),
  connectionPill: document.getElementById('connectionPill'),
  readerPill: document.getElementById('readerPill'),
  disconnectedSplash: document.getElementById('disconnectedSplash'),
  liveWorkspace: document.getElementById('liveWorkspace'),
  baudRateInput: document.getElementById('baudRateInput'),
  pollRateInput: document.getElementById('pollRateInput'),
  autoPollToggle: document.getElementById('autoPollToggle'),
  logOutput: document.getElementById('logOutput'),
  logLineTemplate: document.getElementById('logLineTemplate'),
  clearLogButton: document.getElementById('clearLogButton'),
  manualCommandForm: document.getElementById('manualCommandForm'),
  manualCommandInput: document.getElementById('manualCommandInput'),
  moveForm: document.getElementById('moveForm'),
  moveAbsoluteInput: document.getElementById('moveAbsoluteInput'),
  moveRelativeForm: document.getElementById('moveRelativeForm'),
  moveRelativeInput: document.getElementById('moveRelativeInput'),
  pressTargetInput: document.getElementById('pressTargetInput'),
  cycleCountInput: document.getElementById('cycleCountInput'),
  setPressTargetButton: document.getElementById('setPressTargetButton'),
  startCycleButton: document.getElementById('startCycleButton'),
  irunInput: document.getElementById('irunInput'),
  iholdInput: document.getElementById('iholdInput'),
  iholdDelayInput: document.getElementById('iholdDelayInput'),
  sgthrsInput: document.getElementById('sgthrsInput'),
  setIrunButton: document.getElementById('setIrunButton'),
  setIholdButton: document.getElementById('setIholdButton'),
  setIholdDelayButton: document.getElementById('setIholdDelayButton'),
  setSgthrsButton: document.getElementById('setSgthrsButton'),
  simLoadInput: document.getElementById('simLoadInput'),
  simThresholdInput: document.getElementById('simThresholdInput'),
  setSimLoadButton: document.getElementById('setSimLoadButton'),
  setSimThresholdButton: document.getElementById('setSimThresholdButton'),
  macroEditor: document.getElementById('macroEditor'),
  runMacroButton: document.getElementById('runMacroButton'),
  stopMacroButton: document.getElementById('stopMacroButton'),
  loadRoutinePresetButton: document.getElementById('loadRoutinePresetButton'),
  clearChartButton: document.getElementById('clearChartButton'),
  clearLoadCellChartButton: document.getElementById('clearLoadCellChartButton'),
  refreshConfigButton: document.getElementById('refreshConfigButton'),
  saveConfigButton: document.getElementById('saveConfigButton'),
  tareButton: document.getElementById('tareButton'),
  rebootButton: document.getElementById('rebootButton'),
  bootloaderButton: document.getElementById('bootloaderButton'),
  resetConfigButton: document.getElementById('resetConfigButton'),
  loadSimulatedCurveButton: document.getElementById('loadSimulatedCurveButton'),
  clearResponseCurveButton: document.getElementById('clearResponseCurveButton'),
  telemetryChart: document.getElementById('telemetryChart'),
  loadCellChart: document.getElementById('loadCellChart'),
  responseCurveChart: document.getElementById('responseCurveChart'),
  loadCellSummary: document.getElementById('loadCellSummary'),
  responseCurveSummary: document.getElementById('responseCurveSummary'),
  positionValue: document.getElementById('positionValue'),
  targetValue: document.getElementById('targetValue'),
  homedValue: document.getElementById('homedValue'),
  holdValue: document.getElementById('holdValue'),
  forceValue: document.getElementById('forceValue'),
  loadStateValue: document.getElementById('loadStateValue'),
  stopSourceValue: document.getElementById('stopSourceValue'),
  faultValue: document.getElementById('faultValue'),
  cycleRemainingValue: document.getElementById('cycleRemainingValue'),
  cycleDoneValue: document.getElementById('cycleDoneValue'),
  driverCurrentValue: document.getElementById('driverCurrentValue'),
  driverThresholdValue: document.getElementById('driverThresholdValue'),
  loopLastValue: document.getElementById('loopLastValue'),
  loopMaxValue: document.getElementById('loopMaxValue'),
  stepTotalValue: document.getElementById('stepTotalValue'),
  stepWindowValue: document.getElementById('stepWindowValue'),
  loadCellSourceValue: document.getElementById('loadCellSourceValue'),
  loadCellRawValue: document.getElementById('loadCellRawValue'),
  loadCellThresholdValue: document.getElementById('loadCellThresholdValue'),
  loadCellPeakValue: document.getElementById('loadCellPeakValue'),
  loadCellTriggerValue: document.getElementById('loadCellTriggerValue'),
  configStateValue: document.getElementById('configStateValue'),
  configDirtyValue: document.getElementById('configDirtyValue'),
  configLoadCellProfileValue: document.getElementById('configLoadCellProfileValue'),
  configLoadCellSourceValue: document.getElementById('configLoadCellSourceValue'),
  configLoadCellPinsValue: document.getElementById('configLoadCellPinsValue'),
  configLoadCellThresholdValue: document.getElementById('configLoadCellThresholdValue'),
  configAllowUnverifiedValue: document.getElementById('configAllowUnverifiedValue'),
  configPanelColorValue: document.getElementById('configPanelColorValue'),
  configPanelSwatch: document.getElementById('configPanelSwatch'),
  loadCellConfigForm: document.getElementById('loadCellConfigForm'),
  loadCellConnectorSelect: document.getElementById('loadCellConnectorSelect'),
  loadCellSourceSelect: document.getElementById('loadCellSourceSelect'),
  loadCellThresholdInput: document.getElementById('loadCellThresholdInput'),
  loadCellDataPinInput: document.getElementById('loadCellDataPinInput'),
  loadCellClockPinInput: document.getElementById('loadCellClockPinInput'),
  runtimeConfigForm: document.getElementById('runtimeConfigForm'),
  allowUnverifiedMotionToggle: document.getElementById('allowUnverifiedMotionToggle'),
  panelColorRedInput: document.getElementById('panelColorRedInput'),
  panelColorGreenInput: document.getElementById('panelColorGreenInput'),
  panelColorBlueInput: document.getElementById('panelColorBlueInput'),
  themeModeSelect: document.getElementById('themeModeSelect'),
  themeStatusValue: document.getElementById('themeStatusValue'),
  sidebarConnectionValue: document.getElementById('sidebarConnectionValue'),
  sidebarMachineValue: document.getElementById('sidebarMachineValue'),
  currentViewEyebrow: document.getElementById('currentViewEyebrow'),
  currentViewTitle: document.getElementById('currentViewTitle'),
  currentViewDescription: document.getElementById('currentViewDescription'),
  summaryForcePill: document.getElementById('summaryForcePill'),
  summaryStatePill: document.getElementById('summaryStatePill'),
  viewButtons: Array.from(document.querySelectorAll('[data-view-button]')),
  viewSections: Array.from(document.querySelectorAll('.dashboard-view'))
};

const state = {
  port: null,
  reader: null,
  writer: null,
  readAbortController: null,
  isConnected: false,
  isReading: false,
  pollTimer: null,
  macroAbort: false,
  telemetry: {
    pos: 0,
    target: 0,
    homed: 0,
    hold: 0,
    force: 0,
    load: 0,
    mech: 0,
    stall: 0,
    source: 0,
    fault: 0,
    cycles: 0,
    done: 0,
    loopLastUs: 0,
    loopMaxUs: 0,
    stepsTotal: 0,
    stepsHeartbeat: 0,
    stepsBurst: 0,
    tmcSync: 0,
    driver: { uart: null, irun: null, ihold: null, iholddelay: null, tpowerdown: null, sgthrs: null }
  },
  chartSamples: [],
  loadCell: {
    source: '--',
    raw: 0,
    threshold: 0,
    triggered: 0,
    peak: 0
  },
  loadCellSamples: [],
  simulatedCurve: {
    meta: null,
    samples: []
  },
  activeView: 'overview',
  themeMode: 'auto',
  config: {
    loaded: null,
    dirty: null,
    rebootRequired: null,
    allowUnverifiedMotion: null,
    loadCell: {
      source: 'hx711',
      connector: 'custom',
      threshold: 1000,
      dataPin: 'PE4',
      clockPin: 'PE5'
    },
    panelColor: { red: 255, green: 0, blue: 0 }
  }
};

const VIEW_META = {
  overview: {
    eyebrow: 'Overview',
    title: 'Overview',
    description: 'Live machine state, charts, and bench telemetry at a glance.'
  },
  controls: {
    eyebrow: 'Controls',
    title: 'Controls & Setup',
    description: 'Tune runtime behavior, adjust load-cell wiring and thresholds, and drive the mechanism directly.'
  },
  automation: {
    eyebrow: 'Automation',
    title: 'Automation & Console',
    description: 'Preview simulated response curves, run macros, and drive raw command workflows from one place.'
  }
};

const systemThemeMedia = window.matchMedia('(prefers-color-scheme: dark)');

const STOP_SOURCE_LABELS = {
  0: 'None',
  1: 'Load Cell',
  2: 'Mechanical',
  3: 'Stall',
  4: 'Seek Fault',
  5: 'Travel Fault'
};

function logLine(kind, message) {
  const fragment = dom.logLineTemplate.content.cloneNode(true);
  fragment.querySelector('.log-time').textContent = new Date().toLocaleTimeString();
  fragment.querySelector('.log-kind').textContent = kind;
  fragment.querySelector('.log-message').textContent = message;
  dom.logOutput.prepend(fragment);

  while (dom.logOutput.childElementCount > 300) {
    dom.logOutput.removeChild(dom.logOutput.lastElementChild);
  }
}

function updateConnectionUi() {
  dom.connectButton.disabled = state.isConnected;
  dom.disconnectButton.disabled = !state.isConnected;
  dom.connectionPill.textContent = state.isConnected ? 'Connected' : 'Disconnected';
  dom.connectionPill.className = `pill ${state.isConnected ? '' : 'pill-offline'}`.trim();
  dom.readerPill.textContent = state.isReading ? 'Streaming' : 'Idle';
  dom.sidebarConnectionValue.textContent = state.isConnected ? 'Connected' : 'Disconnected';
  document.body.classList.toggle('is-disconnected', !state.isConnected);
}

function updateViewSummary() {
  const view = VIEW_META[state.activeView] ?? VIEW_META.overview;
  const stopLabel = STOP_SOURCE_LABELS[state.telemetry.source] ?? `Code ${state.telemetry.source}`;
  const loadState = state.telemetry.load ? 'Load active' : (state.telemetry.mech ? 'Mechanical trip' : (state.telemetry.stall ? 'Stall trip' : 'Load clear'));

  dom.currentViewEyebrow.textContent = view.eyebrow;
  dom.currentViewTitle.textContent = view.title;
  dom.currentViewDescription.textContent = view.description;
  dom.summaryForcePill.textContent = `Force ${state.telemetry.force}`;
  dom.summaryStatePill.textContent = `${loadState} / ${stopLabel}`;
}

function setActiveView(viewName) {
  state.activeView = VIEW_META[viewName] ? viewName : 'overview';
  dom.viewButtons.forEach((button) => {
    button.classList.toggle('is-active', button.dataset.viewButton === state.activeView);
  });
  dom.viewSections.forEach((section) => {
    section.classList.toggle('is-active', section.dataset.view === state.activeView);
  });
  updateViewSummary();
}

function resolveTheme() {
  if (state.themeMode === 'dark') {
    return 'dark';
  }

  if (state.themeMode === 'light') {
    return 'light';
  }

  return systemThemeMedia.matches ? 'dark' : 'light';
}

function applyTheme() {
  const resolvedTheme = resolveTheme();
  document.documentElement.dataset.theme = resolvedTheme;
  if (state.themeMode === 'auto') {
    dom.themeStatusValue.textContent = `Following system: ${resolvedTheme}.`;
  } else {
    dom.themeStatusValue.textContent = `Forced ${resolvedTheme} mode.`;
  }
}

function parseKeyValueLine(line) {
  return Object.fromEntries(
    line
      .trim()
      .split(/\s+/)
      .map((pair) => pair.split('='))
      .filter((parts) => parts.length === 2)
      .map(([key, value]) => [key, Number(value)])
  );
}

function parseSimTelemetryLine(line) {
  const result = {};

  line
    .replace(/^sim\s+/, '')
    .trim()
    .split(/\s+/)
    .forEach((pair) => {
      const [key, rawValue] = pair.split('=');
      if (!key || rawValue === undefined) {
        return;
      }

      if (key === 'source') {
        result.source = rawValue;
        return;
      }

      const numericValue = Number(rawValue);
      result[key] = Number.isFinite(numericValue) ? numericValue : rawValue;
    });

  return result;
}

function updateTelemetryCardValues() {
  const driver = state.telemetry.driver;
  const displayDriverValue = (value) => (Number.isFinite(value) ? String(value) : '--');

  dom.positionValue.textContent = String(state.telemetry.pos);
  dom.targetValue.textContent = `Target ${state.telemetry.target}`;
  dom.homedValue.textContent = state.telemetry.homed ? 'Yes' : 'No';
  dom.holdValue.textContent = state.telemetry.hold ? 'Hold on' : 'Hold off';
  dom.forceValue.textContent = String(state.telemetry.force);
  dom.loadStateValue.textContent = `Load ${state.telemetry.load} / Mech ${state.telemetry.mech} / Stall ${state.telemetry.stall}`;
  dom.stopSourceValue.textContent = STOP_SOURCE_LABELS[state.telemetry.source] ?? `Code ${state.telemetry.source}`;
  dom.faultValue.textContent = `Fault ${state.telemetry.fault}`;
  dom.cycleRemainingValue.textContent = String(state.telemetry.cycles);
  dom.cycleDoneValue.textContent = `Done ${state.telemetry.done}`;
  dom.driverCurrentValue.textContent = `I${displayDriverValue(driver.irun)} / H${displayDriverValue(driver.ihold)}`;
  dom.driverThresholdValue.textContent = `SG ${displayDriverValue(driver.sgthrs)} / UART ${displayDriverValue(driver.uart)}`;
  dom.loopLastValue.textContent = `${state.telemetry.loopLastUs} us`;
  dom.loopMaxValue.textContent = `Max ${state.telemetry.loopMaxUs} us`;
  dom.stepTotalValue.textContent = String(state.telemetry.stepsTotal);
  dom.stepWindowValue.textContent = `HB ${state.telemetry.stepsHeartbeat} / Burst ${state.telemetry.stepsBurst} / Sync ${state.telemetry.tmcSync}`;
  dom.sidebarMachineValue.textContent = `Force ${state.telemetry.force} / ${state.telemetry.hold ? 'Hold on' : 'Hold off'}`;

  if (Number.isFinite(driver.irun)) {
    dom.irunInput.value = String(driver.irun);
  }
  if (Number.isFinite(driver.ihold)) {
    dom.iholdInput.value = String(driver.ihold);
  }
  if (Number.isFinite(driver.iholddelay)) {
    dom.iholdDelayInput.value = String(driver.iholddelay);
  }
  if (Number.isFinite(driver.sgthrs)) {
    dom.sgthrsInput.value = String(driver.sgthrs);
  }

  updateViewSummary();
}

function updateLoadCellValues() {
  dom.loadCellSourceValue.textContent = state.loadCell.source;
  dom.loadCellRawValue.textContent = String(state.loadCell.raw);
  dom.loadCellThresholdValue.textContent = String(state.loadCell.threshold);
  dom.loadCellPeakValue.textContent = String(state.loadCell.peak);
  dom.loadCellTriggerValue.textContent = state.loadCell.triggered ? 'Triggered' : 'Clear';
  dom.loadCellTriggerValue.classList.toggle('is-triggered', Boolean(state.loadCell.triggered));
  dom.loadCellSummary.textContent = `Live ${state.loadCell.source} samples. Raw ${state.loadCell.raw}, threshold ${state.loadCell.threshold}, peak ${state.loadCell.peak}, trigger ${state.loadCell.triggered ? 'active' : 'clear'}.`;
}

function updateConfigValues() {
  const loadedText = state.config.loaded === null ? 'Unknown' : (state.config.loaded ? 'Loaded' : 'Defaults');
  const dirtyText = state.config.dirty === null ? '--' : (state.config.dirty ? 'Yes' : 'No');
  const rebootText = state.config.rebootRequired === null ? '--' : (state.config.rebootRequired ? 'Yes' : 'No');
  const allowText = state.config.allowUnverifiedMotion === null ? '--' : (state.config.allowUnverifiedMotion ? 'Enabled' : 'Verified only');
  const panelRgbText = `${state.config.panelColor.red}, ${state.config.panelColor.green}, ${state.config.panelColor.blue}`;

  dom.configStateValue.textContent = loadedText;
  dom.configDirtyValue.textContent = `Dirty ${dirtyText} / Reboot ${rebootText}`;
  dom.configLoadCellProfileValue.textContent = state.config.loadCell.connector.toUpperCase();
  dom.configLoadCellSourceValue.textContent = `Source ${state.config.loadCell.source.toUpperCase()}`;
  dom.configLoadCellPinsValue.textContent = `${state.config.loadCell.dataPin} / ${state.config.loadCell.clockPin}`;
  dom.configLoadCellThresholdValue.textContent = `Threshold ${state.config.loadCell.threshold}`;
  dom.configAllowUnverifiedValue.textContent = allowText;
  dom.configPanelColorValue.textContent = panelRgbText;
  dom.configPanelSwatch.style.background = `rgb(${state.config.panelColor.red}, ${state.config.panelColor.green}, ${state.config.panelColor.blue})`;

  dom.loadCellConnectorSelect.value = state.config.loadCell.connector;
  dom.loadCellSourceSelect.value = state.config.loadCell.source;
  dom.loadCellThresholdInput.value = String(state.config.loadCell.threshold);
  dom.loadCellDataPinInput.value = state.config.loadCell.dataPin;
  dom.loadCellClockPinInput.value = state.config.loadCell.clockPin;
  dom.allowUnverifiedMotionToggle.checked = Boolean(state.config.allowUnverifiedMotion);
  dom.panelColorRedInput.value = String(state.config.panelColor.red);
  dom.panelColorGreenInput.value = String(state.config.panelColor.green);
  dom.panelColorBlueInput.value = String(state.config.panelColor.blue);

  updateLoadCellConfigUi();
}

function updateLoadCellConfigUi() {
  const isCustom = dom.loadCellConnectorSelect.value === 'custom';
  const isHx711 = dom.loadCellSourceSelect.value === 'hx711';

  dom.loadCellSourceSelect.disabled = !isCustom;
  dom.loadCellDataPinInput.disabled = !isCustom;
  dom.loadCellClockPinInput.disabled = !isCustom || !isHx711;
}

function parseConfigLine(line) {
  if (line.startsWith('config loaded=')) {
    const parsed = parseKeyValueLine(line.replace('config ', ''));
    state.config.loaded = parsed.loaded ?? state.config.loaded;
    state.config.dirty = parsed.dirty ?? state.config.dirty;
    state.config.rebootRequired = parsed.reboot ?? state.config.rebootRequired;
    updateConfigValues();
    return true;
  }

  if (line.startsWith('config loadcell.source=')) {
    const match = line.match(/^config loadcell\.source=([^ ]+) loadcell\.connector=([^ ]+) loadcell\.threshold=(\d+) pin\.loadcell_data=([^ ]+) pin\.loadcell_clock=([^ ]+)$/);
    if (!match) {
      return false;
    }

    state.config.loadCell.source = match[1].toLowerCase();
    state.config.loadCell.connector = match[2].toLowerCase();
    state.config.loadCell.threshold = Number(match[3]);
    state.config.loadCell.dataPin = match[4].toUpperCase();
    state.config.loadCell.clockPin = match[5].toUpperCase();
    updateConfigValues();
    return true;
  }

  if (line.startsWith('config telemetry.status_interval_ms=')) {
    const allowMatch = line.match(/tmc\.allow_unverified_motion=(\d+)/);
    const panelMatch = line.match(/panel\.color_rgb=(\d+),(\d+),(\d+)/);

    if (allowMatch) {
      state.config.allowUnverifiedMotion = Number(allowMatch[1]);
    }
    if (panelMatch) {
      state.config.panelColor.red = Number(panelMatch[1]);
      state.config.panelColor.green = Number(panelMatch[2]);
      state.config.panelColor.blue = Number(panelMatch[3]);
    }

    updateConfigValues();
    return true;
  }

  return false;
}

function pushChartSample(force, position) {
  state.chartSamples.push({
    at: Date.now(),
    force,
    position
  });

  while (state.chartSamples.length > 180) {
    state.chartSamples.shift();
  }

  renderChart();
}

function pushLoadCellSample(raw, threshold, triggered) {
  state.loadCellSamples.push({
    at: Date.now(),
    raw,
    threshold,
    triggered
  });

  while (state.loadCellSamples.length > 360) {
    state.loadCellSamples.shift();
  }

  state.loadCell.raw = raw;
  state.loadCell.threshold = threshold;
  state.loadCell.triggered = triggered;
  state.loadCell.peak = Math.max(state.loadCell.peak, raw);
  updateLoadCellValues();
  renderLoadCellChart();
}

function formatCurveSummary(meta) {
  if (!meta) {
    return 'Run test\\run_simulated_probe_workflow.ps1, then load the exported curve here to preview the simulated switch/load-cell response before hardware hookup.';
  }

  return `Threshold ${meta.threshold}, press target ${meta.pressTarget}, contact ${meta.contactPosition}, cycles ${meta.cycleCount}, hits ${meta.loadCellHits}, max force ${meta.maxForce}.`;
}

function renderChart() {
  const canvas = dom.telemetryChart;
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  ctx.clearRect(0, 0, width, height);

  ctx.fillStyle = '#fefaf4';
  ctx.fillRect(0, 0, width, height);

  ctx.strokeStyle = 'rgba(60, 40, 24, 0.12)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i += 1) {
    const y = 24 + ((height - 48) / 4) * i;
    ctx.beginPath();
    ctx.moveTo(18, y);
    ctx.lineTo(width - 18, y);
    ctx.stroke();
  }

  const samples = state.chartSamples;
  if (samples.length < 2) {
    ctx.fillStyle = '#6a5a4d';
    ctx.font = '16px Bahnschrift';
    ctx.fillText('Waiting for telemetry…', 24, 42);
    return;
  }

  const forceValues = samples.map((sample) => sample.force);
  const posValues = samples.map((sample) => sample.position);
  const forceMin = Math.min(...forceValues, 0);
  const forceMax = Math.max(...forceValues, 1);
  const posMin = Math.min(...posValues, 0);
  const posMax = Math.max(...posValues, 1);
  const left = 18;
  const top = 18;
  const chartWidth = width - 36;
  const chartHeight = height - 48;

  const xAt = (index) => left + (chartWidth * index) / Math.max(samples.length - 1, 1);
  const mapY = (value, minValue, maxValue) => {
    const spread = Math.max(maxValue - minValue, 1);
    return top + chartHeight - ((value - minValue) / spread) * chartHeight;
  };

  ctx.lineWidth = 2.5;
  ctx.strokeStyle = '#b34c31';
  ctx.beginPath();
  samples.forEach((sample, index) => {
    const x = xAt(index);
    const y = mapY(sample.force, forceMin, forceMax);
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();

  ctx.strokeStyle = '#1c8b7d';
  ctx.beginPath();
  samples.forEach((sample, index) => {
    const x = xAt(index);
    const y = mapY(sample.position, posMin, posMax);
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();

  ctx.fillStyle = '#b34c31';
  ctx.font = '13px Bahnschrift';
  ctx.fillText(`Force ${forceMin}..${forceMax}`, 24, height - 12);
  ctx.fillStyle = '#1c8b7d';
  ctx.fillText(`Position ${posMin}..${posMax}`, 180, height - 12);
}

function renderLoadCellChart() {
  const canvas = dom.loadCellChart;
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  const left = 52;
  const top = 18;
  const chartWidth = width - 76;
  const chartHeight = height - 52;

  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#fefaf4';
  ctx.fillRect(0, 0, width, height);

  ctx.strokeStyle = 'rgba(60, 40, 24, 0.12)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i += 1) {
    const y = top + (chartHeight / 4) * i;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(left + chartWidth, y);
    ctx.stroke();
  }

  const samples = state.loadCellSamples;
  if (samples.length < 2) {
    ctx.fillStyle = '#6a5a4d';
    ctx.font = '16px Bahnschrift';
    ctx.fillText('Waiting for live load-cell samples…', 24, 42);
    return;
  }

  const rawValues = samples.map((sample) => sample.raw);
  const thresholdValues = samples.map((sample) => sample.threshold);
  const yMin = Math.min(...rawValues, ...thresholdValues, 0);
  const yMax = Math.max(...rawValues, ...thresholdValues, 1);
  const xAt = (index) => left + (chartWidth * index) / Math.max(samples.length - 1, 1);
  const yAt = (value) => top + chartHeight - ((value - yMin) / Math.max(yMax - yMin, 1)) * chartHeight;

  const latestThreshold = samples[samples.length - 1]?.threshold ?? 0;
  const thresholdY = yAt(latestThreshold);
  ctx.setLineDash([6, 4]);
  ctx.strokeStyle = 'rgba(179, 76, 49, 0.9)';
  ctx.beginPath();
  ctx.moveTo(left, thresholdY);
  ctx.lineTo(left + chartWidth, thresholdY);
  ctx.stroke();
  ctx.setLineDash([]);

  ctx.fillStyle = 'rgba(28, 139, 125, 0.12)';
  let activeStartIndex = null;
  samples.forEach((sample, index) => {
    if (sample.triggered && activeStartIndex === null) {
      activeStartIndex = index;
    }

    const isLast = index === samples.length - 1;
    if ((!sample.triggered || isLast) && activeStartIndex !== null) {
      const endIndex = sample.triggered && isLast ? index : Math.max(index - 1, activeStartIndex);
      const startX = xAt(activeStartIndex);
      const endX = xAt(endIndex);
      ctx.fillRect(startX, top, Math.max(endX - startX, 3), chartHeight);
      activeStartIndex = null;
    }
  });

  ctx.lineWidth = 2.5;
  ctx.strokeStyle = '#1c8b7d';
  ctx.beginPath();
  samples.forEach((sample, index) => {
    const x = xAt(index);
    const y = yAt(sample.raw);
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();

  ctx.fillStyle = '#1c8b7d';
  samples.filter((sample) => sample.triggered).forEach((sample, index) => {
    const sampleIndex = samples.indexOf(sample);
    ctx.beginPath();
    ctx.arc(xAt(sampleIndex), yAt(sample.raw), 3, 0, Math.PI * 2);
    ctx.fill();
  });

  ctx.fillStyle = '#6a5a4d';
  ctx.font = '13px Bahnschrift';
  ctx.fillText(`Raw ${yMin}..${yMax}`, left, height - 12);
  ctx.fillStyle = '#b34c31';
  ctx.fillText(`Threshold ${latestThreshold}`, 220, height - 12);
  ctx.fillStyle = '#1c8b7d';
  ctx.fillText(`Peak ${state.loadCell.peak}`, 390, height - 12);
}

function renderResponseCurve() {
  const canvas = dom.responseCurveChart;
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  ctx.clearRect(0, 0, width, height);

  ctx.fillStyle = '#fefaf4';
  ctx.fillRect(0, 0, width, height);

  const left = 52;
  const top = 18;
  const chartWidth = width - 76;
  const chartHeight = height - 52;

  ctx.strokeStyle = 'rgba(60, 40, 24, 0.12)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i += 1) {
    const y = top + (chartHeight / 4) * i;
    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(left + chartWidth, y);
    ctx.stroke();
  }

  const samples = state.simulatedCurve.samples;
  const meta = state.simulatedCurve.meta;
  dom.responseCurveSummary.textContent = formatCurveSummary(meta);

  if (!samples.length) {
    ctx.fillStyle = '#6a5a4d';
    ctx.font = '16px Bahnschrift';
    ctx.fillText('No simulated curve loaded yet.', 24, 42);
    return;
  }

  const positions = samples.map((sample) => sample.position);
  const forces = samples.map((sample) => sample.force);
  const xMin = Math.min(...positions, 0);
  const xMax = Math.max(...positions, 1);
  const yMin = 0;
  const yMax = Math.max(...forces, meta?.threshold ?? 0, 1);
  const xAt = (value) => left + ((value - xMin) / Math.max(xMax - xMin, 1)) * chartWidth;
  const yAt = (value) => top + chartHeight - ((value - yMin) / Math.max(yMax - yMin, 1)) * chartHeight;

  if (meta && Number.isFinite(meta.threshold)) {
    const thresholdY = yAt(meta.threshold);
    ctx.setLineDash([6, 5]);
    ctx.strokeStyle = 'rgba(179, 76, 49, 0.9)';
    ctx.beginPath();
    ctx.moveTo(left, thresholdY);
    ctx.lineTo(left + chartWidth, thresholdY);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#b34c31';
    ctx.font = '13px Bahnschrift';
    ctx.fillText(`Threshold ${meta.threshold}`, left + 8, Math.max(thresholdY - 8, 18));
  }

  if (meta && Number.isFinite(meta.contactPosition)) {
    const contactX = xAt(meta.contactPosition);
    ctx.setLineDash([3, 4]);
    ctx.strokeStyle = 'rgba(28, 139, 125, 0.6)';
    ctx.beginPath();
    ctx.moveTo(contactX, top);
    ctx.lineTo(contactX, top + chartHeight);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  ctx.lineWidth = 2.5;
  ctx.strokeStyle = '#1c8b7d';
  ctx.beginPath();
  samples.forEach((sample, index) => {
    const x = xAt(sample.position);
    const y = yAt(sample.force);
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  ctx.stroke();

  ctx.fillStyle = '#b34c31';
  samples.filter((sample) => sample.load).forEach((sample) => {
    ctx.beginPath();
    ctx.arc(xAt(sample.position), yAt(sample.force), 3.5, 0, Math.PI * 2);
    ctx.fill();
  });

  ctx.fillStyle = '#6a5a4d';
  ctx.font = '13px Bahnschrift';
  ctx.fillText(`Position ${xMin}..${xMax}`, left, height - 12);
  ctx.fillText(`Force ${yMin}..${yMax}`, 220, height - 12);
}

async function loadSimulatedCurve() {
  const response = await fetch(`./simulated-probe-curve.json?ts=${Date.now()}`);
  if (!response.ok) {
    throw new Error(`Simulated curve file was not found (${response.status}). Run test\\run_simulated_probe_workflow.ps1 first.`);
  }

  const payload = await response.json();
  state.simulatedCurve.meta = payload.meta ?? null;
  state.simulatedCurve.samples = Array.isArray(payload.samples) ? payload.samples : [];
  renderResponseCurve();
  logLine('sys', `Loaded simulated curve with ${state.simulatedCurve.samples.length} samples.`);
}

function handleTelemetryLine(line) {
  if (parseConfigLine(line)) {
    return true;
  }

  if (line.startsWith('diag0=')) {
    const parsed = parseKeyValueLine(line);
    Object.assign(state.telemetry, {
      force: parsed.force ?? state.telemetry.force,
      load: parsed.load ?? state.telemetry.load,
      mech: parsed.mech ?? state.telemetry.mech,
      stall: parsed.stall ?? state.telemetry.stall,
      source: parsed.source ?? state.telemetry.source,
      fault: parsed.fault ?? state.telemetry.fault,
      homed: parsed.homed ?? state.telemetry.homed,
      hold: parsed.hold ?? state.telemetry.hold,
      pos: parsed.pos ?? state.telemetry.pos,
      target: parsed.target ?? state.telemetry.target,
      cycles: parsed.cycles ?? state.telemetry.cycles,
      done: parsed.done ?? state.telemetry.done,
      loopLastUs: parsed.loop_last_us ?? state.telemetry.loopLastUs,
      loopMaxUs: parsed.loop_max_us ?? state.telemetry.loopMaxUs,
      stepsTotal: parsed.steps_total ?? state.telemetry.stepsTotal,
      stepsHeartbeat: parsed.steps_hb ?? state.telemetry.stepsHeartbeat,
      stepsBurst: parsed.steps_burst ?? state.telemetry.stepsBurst,
      tmcSync: parsed.tmc_sync ?? state.telemetry.tmcSync
    });
    updateTelemetryCardValues();
    pushChartSample(state.telemetry.force, state.telemetry.pos);
    return true;
  }

  if (line.startsWith('heartbeat ')) {
    const parsed = parseKeyValueLine(line.replace('heartbeat ', ''));
    Object.assign(state.telemetry, {
      source: parsed.source ?? state.telemetry.source,
      fault: parsed.fault ?? state.telemetry.fault,
      homed: parsed.homed ?? state.telemetry.homed,
      hold: parsed.hold ?? state.telemetry.hold,
      pos: parsed.pos ?? state.telemetry.pos,
      target: parsed.target ?? state.telemetry.target,
      loopLastUs: parsed.loop_last_us ?? state.telemetry.loopLastUs,
      loopMaxUs: parsed.loop_max_us ?? state.telemetry.loopMaxUs,
      stepsTotal: parsed.steps_total ?? state.telemetry.stepsTotal,
      stepsHeartbeat: parsed.steps_hb ?? state.telemetry.stepsHeartbeat,
      stepsBurst: parsed.steps_burst ?? state.telemetry.stepsBurst,
      tmcSync: parsed.tmc_sync ?? state.telemetry.tmcSync
    });
    updateTelemetryCardValues();
    pushChartSample(state.telemetry.force, state.telemetry.pos);
    return true;
  }

  if (line.startsWith('driver ')) {
    const parsed = parseKeyValueLine(line.replace('driver ', ''));
    Object.assign(state.telemetry.driver, parsed);
    updateTelemetryCardValues();
    return true;
  }

  if (line.startsWith('sim ')) {
    const parsed = parseSimTelemetryLine(line);
    state.loadCell.source = parsed.source ?? state.loadCell.source;
    state.telemetry.load = parsed.load ?? state.telemetry.load;
    state.telemetry.mech = parsed.mech ?? state.telemetry.mech;
    state.telemetry.stall = parsed.stall ?? state.telemetry.stall;
    updateTelemetryCardValues();
    pushLoadCellSample(parsed.raw ?? state.loadCell.raw, parsed.thresh ?? state.loadCell.threshold, parsed.load ?? state.loadCell.triggered);
    return true;
  }

  if (line.startsWith('cmd: set key=')) {
    const match = line.match(/^cmd: set key=([^ ]+) value=(.+) ok=(\d+)(?: reboot=(\d+))?$/);
    if (match && Number(match[3]) === 1) {
      state.config.dirty = 1;
      if (match[4] !== undefined) {
        state.config.rebootRequired = Number(match[4]);
      }
      updateConfigValues();
    }
    return false;
  }

  if (line.startsWith('cmd: save ')) {
    const match = line.match(/^cmd: save ok=(\d+) reboot=(\d+)$/);
    if (match && Number(match[1]) === 1) {
      state.config.loaded = 1;
      state.config.dirty = 0;
      state.config.rebootRequired = Number(match[2]);
      updateConfigValues();
    }
    return false;
  }

  if (line.startsWith('cmd: resetcfg ')) {
    state.config.dirty = 1;
    state.config.rebootRequired = 1;
    updateConfigValues();
    return false;
  }

  return false;
}

async function sendCommand(command) {
  if (!state.isConnected || !state.writer) {
    throw new Error('Serial port is not connected.');
  }

  const payload = `${command.trim()}\r\n`;
  await state.writer.write(new TextEncoder().encode(payload));
  logLine('tx', command.trim());
}

async function runCommandSequence(commands, delayMs = 120) {
  for (const command of commands) {
    await sendCommand(command);
    await new Promise((resolve) => window.setTimeout(resolve, delayMs));
  }
}

async function refreshConfigState() {
  await sendCommand('config');
}

async function disconnectSerial() {
  state.macroAbort = true;
  clearInterval(state.pollTimer);
  state.pollTimer = null;

  if (state.reader) {
    try {
      await state.reader.cancel();
    } catch {
      // ignore cancellation races
    }
  }

  if (state.writer) {
    try {
      state.writer.releaseLock();
    } catch {
      // ignore
    }
    state.writer = null;
  }

  if (state.port) {
    try {
      await state.port.close();
    } catch {
      // ignore
    }
  }

  state.port = null;
  state.reader = null;
  state.isConnected = false;
  state.isReading = false;
  updateConnectionUi();
}

async function startReadLoop() {
  state.isReading = true;
  updateConnectionUi();

  const decoder = new TextDecoder();
  let pending = '';

  try {
    while (state.port?.readable) {
      state.reader = state.port.readable.getReader();
      try {
        for (;;) {
          const { value, done } = await state.reader.read();
          if (done) {
            break;
          }
          pending += decoder.decode(value, { stream: true });
          const lines = pending.split(/\r?\n/);
          pending = lines.pop() ?? '';
          lines
            .map((line) => line.trim())
            .filter(Boolean)
            .forEach((line) => {
              const parsedAsTelemetry = handleTelemetryLine(line);
              logLine(parsedAsTelemetry ? 'rx' : 'evt', line);
            });
        }
      } finally {
        state.reader.releaseLock();
        state.reader = null;
      }
      break;
    }
  } catch (error) {
    logLine('err', error.message || String(error));
  } finally {
    state.isReading = false;
    updateConnectionUi();
    if (state.port) {
      await disconnectSerial();
    }
  }
}

function startPolling() {
  clearInterval(state.pollTimer);
  if (!dom.autoPollToggle.checked) {
    return;
  }

  const interval = Math.max(Number(dom.pollRateInput.value) || 1500, 250);
  state.pollTimer = window.setInterval(async () => {
    if (!state.isConnected) {
      return;
    }
    try {
      await sendCommand('safety');
      await sendCommand('driver');
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  }, interval);
}

function scheduleInitialRefresh() {
  window.setTimeout(() => {
    runCommandSequence(['config', 'status', 'safety', 'driver'], 120).catch((error) => logLine('err', error.message || String(error)));
  }, 180);
}

async function connectSerial() {
  if (!('serial' in navigator)) {
    throw new Error('This browser does not support Web Serial. Use Chrome or Edge on localhost.');
  }

  state.port = await navigator.serial.requestPort();
  await state.port.open({ baudRate: Number(dom.baudRateInput.value) || 115200 });
  state.writer = state.port.writable.getWriter();
  state.isConnected = true;
  state.macroAbort = false;
  updateConnectionUi();
  logLine('sys', 'Serial connection opened.');
  startPolling();
  startReadLoop();
  scheduleInitialRefresh();
}

async function applyLoadCellConfig() {
  const connector = dom.loadCellConnectorSelect.value.trim().toLowerCase();
  const source = dom.loadCellSourceSelect.value.trim().toLowerCase();
  const threshold = Math.max(Number(dom.loadCellThresholdInput.value) || 1, 1);
  const dataPin = dom.loadCellDataPinInput.value.trim().toUpperCase();
  const clockPin = dom.loadCellClockPinInput.value.trim().toUpperCase();
  const commands = [];

  if (connector !== 'custom') {
    commands.push(`set loadcell.connector ${connector}`);
  } else {
    commands.push(`set loadcell.source ${source}`);
    commands.push(`set pin.loadcell_data ${dataPin}`);
    if (source === 'hx711' && clockPin) {
      commands.push(`set pin.loadcell_clock ${clockPin}`);
    }
  }

  commands.push(`set loadcell.threshold ${threshold}`);
  await runCommandSequence(commands, 120);
  await refreshConfigState();
}

async function applyRuntimeConfig() {
  const allowUnverified = dom.allowUnverifiedMotionToggle.checked ? 1 : 0;
  const red = Math.min(Math.max(Number(dom.panelColorRedInput.value) || 0, 0), 255);
  const green = Math.min(Math.max(Number(dom.panelColorGreenInput.value) || 0, 0), 255);
  const blue = Math.min(Math.max(Number(dom.panelColorBlueInput.value) || 0, 0), 255);

  await runCommandSequence([
    `set tmc.allow_unverified_motion ${allowUnverified}`,
    `set panel.color_red ${red}`,
    `set panel.color_green ${green}`,
    `set panel.color_blue ${blue}`
  ], 120);
  await refreshConfigState();
}

async function runMacro() {
  if (!state.isConnected) {
    logLine('err', 'Connect to the board before running a macro.');
    return;
  }

  state.macroAbort = false;
  const lines = dom.macroEditor.value.split(/\r?\n/);
  logLine('sys', `Running macro with ${lines.length} lines.`);

  for (const rawLine of lines) {
    if (state.macroAbort) {
      logLine('sys', 'Macro cancelled.');
      return;
    }

    const line = rawLine.trim();
    if (!line || line.startsWith('#')) {
      continue;
    }

    const delayMatch = line.match(/^DELAY\s+(\d+)$/i);
    if (delayMatch) {
      await new Promise((resolve) => window.setTimeout(resolve, Number(delayMatch[1])));
      continue;
    }

    await sendCommand(line);
    await new Promise((resolve) => window.setTimeout(resolve, 120));
  }

  logLine('sys', 'Macro finished.');
}

function bindCommandButtons() {
  document.querySelectorAll('.command-chip').forEach((button) => {
    button.addEventListener('click', async () => {
      const command = button.dataset.command;
      if (!command) {
        return;
      }
      try {
        await sendCommand(command);
      } catch (error) {
        logLine('err', error.message || String(error));
      }
    });
  });
}

function bindUi() {
  dom.viewButtons.forEach((button) => {
    button.addEventListener('click', () => {
      setActiveView(button.dataset.viewButton);
    });
  });

  dom.themeModeSelect.addEventListener('change', () => {
    state.themeMode = dom.themeModeSelect.value;
    applyTheme();
  });

  const onSystemThemeChange = () => {
    if (state.themeMode === 'auto') {
      applyTheme();
    }
  };

  if (typeof systemThemeMedia.addEventListener === 'function') {
    systemThemeMedia.addEventListener('change', onSystemThemeChange);
  } else if (typeof systemThemeMedia.addListener === 'function') {
    systemThemeMedia.addListener(onSystemThemeChange);
  }

  dom.connectButton.addEventListener('click', async () => {
    try {
      await connectSerial();
    } catch (error) {
      logLine('err', error.message || String(error));
      await disconnectSerial();
    }
  });

  dom.disconnectButton.addEventListener('click', async () => {
    await disconnectSerial();
    logLine('sys', 'Serial connection closed.');
  });

  dom.clearLogButton.addEventListener('click', () => {
    dom.logOutput.innerHTML = '';
  });

  dom.clearChartButton.addEventListener('click', () => {
    state.chartSamples = [];
    renderChart();
  });

  dom.clearLoadCellChartButton.addEventListener('click', () => {
    state.loadCellSamples = [];
    state.loadCell.peak = state.loadCell.raw;
    updateLoadCellValues();
    renderLoadCellChart();
  });

  dom.loadSimulatedCurveButton.addEventListener('click', () => {
    loadSimulatedCurve().catch((error) => logLine('err', error.message || String(error)));
  });

  dom.clearResponseCurveButton.addEventListener('click', () => {
    state.simulatedCurve = { meta: null, samples: [] };
    renderResponseCurve();
  });

  dom.autoPollToggle.addEventListener('change', startPolling);
  dom.pollRateInput.addEventListener('change', startPolling);
  dom.loadCellConnectorSelect.addEventListener('change', updateLoadCellConfigUi);
  dom.loadCellSourceSelect.addEventListener('change', updateLoadCellConfigUi);

  dom.manualCommandForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const command = dom.manualCommandInput.value.trim();
    if (!command) {
      return;
    }
    try {
      await sendCommand(command);
      dom.manualCommandInput.value = '';
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.moveForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    try {
      await sendCommand(`g0 x${dom.moveAbsoluteInput.value}`);
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.moveRelativeForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    try {
      await sendCommand(`jog ${dom.moveRelativeInput.value}`);
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.setPressTargetButton.addEventListener('click', () => sendCommand(`presspos ${dom.pressTargetInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.startCycleButton.addEventListener('click', () => sendCommand(`cycle ${dom.cycleCountInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setIrunButton.addEventListener('click', () => sendCommand(`irun ${dom.irunInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setIholdButton.addEventListener('click', () => sendCommand(`ihold ${dom.iholdInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setIholdDelayButton.addEventListener('click', () => sendCommand(`iholddelay ${dom.iholdDelayInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setSgthrsButton.addEventListener('click', () => sendCommand(`sgthrs ${dom.sgthrsInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setSimLoadButton.addEventListener('click', () => sendCommand(`simload ${dom.simLoadInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.setSimThresholdButton.addEventListener('click', () => sendCommand(`simthresh ${dom.simThresholdInput.value}`).catch((error) => logLine('err', error.message || String(error))));
  dom.refreshConfigButton.addEventListener('click', () => refreshConfigState().catch((error) => logLine('err', error.message || String(error))));
  dom.saveConfigButton.addEventListener('click', () => sendCommand('save').then(() => window.setTimeout(() => refreshConfigState().catch((error) => logLine('err', error.message || String(error))), 150)).catch((error) => logLine('err', error.message || String(error))));
  dom.tareButton.addEventListener('click', () => sendCommand('tare').catch((error) => logLine('err', error.message || String(error))));
  dom.rebootButton.addEventListener('click', () => sendCommand('reboot').catch((error) => logLine('err', error.message || String(error))));
  dom.bootloaderButton.addEventListener('click', () => sendCommand('bootloader').catch((error) => logLine('err', error.message || String(error))));
  dom.resetConfigButton.addEventListener('click', async () => {
    if (!window.confirm('Reset persisted config to defaults and mark the board for reboot?')) {
      return;
    }
    try {
      await sendCommand('resetcfg');
      window.setTimeout(() => refreshConfigState().catch((error) => logLine('err', error.message || String(error))), 150);
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.loadCellConfigForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    try {
      await applyLoadCellConfig();
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.runtimeConfigForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    try {
      await applyRuntimeConfig();
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  });

  dom.runMacroButton.addEventListener('click', () => runMacro().catch((error) => logLine('err', error.message || String(error))));
  dom.stopMacroButton.addEventListener('click', () => {
    state.macroAbort = true;
  });

  dom.loadRoutinePresetButton.addEventListener('click', () => {
    dom.macroEditor.value = [
      '# Endurance routine preset',
      'driver',
      'status',
      'setpos 0',
      'hold on',
      'presspos 40',
      'cycle 1000'
    ].join('\n');
  });

  bindCommandButtons();
}

bindUi();
updateConnectionUi();
updateTelemetryCardValues();
updateLoadCellValues();
updateConfigValues();
setActiveView(state.activeView);
dom.themeModeSelect.value = state.themeMode;
applyTheme();
renderChart();
renderLoadCellChart();
renderResponseCurve();
logLine('sys', 'Dashboard ready. Serve this folder over localhost and connect with Chrome or Edge.');