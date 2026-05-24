const dom = {
  connectButton: document.getElementById('connectButton'),
  disconnectButton: document.getElementById('disconnectButton'),
  connectionPill: document.getElementById('connectionPill'),
  readerPill: document.getElementById('readerPill'),
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
  telemetryChart: document.getElementById('telemetryChart'),
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
  driverThresholdValue: document.getElementById('driverThresholdValue')
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
    driver: { uart: null, irun: null, ihold: null, iholddelay: null, tpowerdown: null, sgthrs: null }
  },
  chartSamples: []
};

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

function handleTelemetryLine(line) {
  if (line.startsWith('pc0=')) {
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
      done: parsed.done ?? state.telemetry.done
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
      target: parsed.target ?? state.telemetry.target
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
    const parsed = parseKeyValueLine(line.replace('sim ', ''));
    state.telemetry.force = parsed.raw ?? state.telemetry.force;
    state.telemetry.load = parsed.load ?? state.telemetry.load;
    state.telemetry.mech = parsed.mech ?? state.telemetry.mech;
    state.telemetry.stall = parsed.stall ?? state.telemetry.stall;
    updateTelemetryCardValues();
    pushChartSample(state.telemetry.force, state.telemetry.pos);
    return true;
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
      await sendCommand('status');
      await sendCommand('driver');
    } catch (error) {
      logLine('err', error.message || String(error));
    }
  }, interval);
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

  dom.autoPollToggle.addEventListener('change', startPolling);
  dom.pollRateInput.addEventListener('change', startPolling);

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
renderChart();
logLine('sys', 'Dashboard ready. Serve this folder over localhost and connect with Chrome or Edge.');