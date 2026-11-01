import * as Dialog from "@radix-ui/react-dialog";
import * as Select from "@radix-ui/react-select";
import * as Slider from "@radix-ui/react-slider";
import * as Switch from "@radix-ui/react-switch";
import * as Tabs from "@radix-ui/react-tabs";
import {
  AlertTriangle,
  Check,
  ChevronDown,
  ChevronUp,
  Crosshair,
  FileCode2,
  FolderOpen,
  Gamepad2,
  HardDrive,
  Keyboard,
  Maximize2,
  Minus,
  Play,
  Radio,
  RotateCcw,
  ShieldCheck,
  Settings2,
  SlidersHorizontal,
  Square,
  Terminal,
  X,
} from "lucide-react";
import { useEffect, useMemo, useRef, useState } from "react";

type TabId = "launch" | "graphics" | "controller" | "system" | "logs";
type Language = "en" | "ja" | "de" | "fr" | "es" | "it";
type SettingKey = "resolution" | "fps" | "language" | "vsync" | "fullscreen" | "gameDir" | "configPath";

const languageNames: Record<Language, string> = {
  en: "English",
  ja: "日本語",
  de: "Deutsch",
  fr: "Français",
  es: "Español",
  it: "Italiano",
};
const descriptions: Record<SettingKey, string> = {
  resolution: "Sets the render window size passed to the engine as --res WIDTHxHEIGHT.",
  fps: "Frame pacing target. Select Unlimited to pass --fps 0.",
  language: "Selects the engine language using --lang.",
  vsync: "Synchronizes frame presentation to your display using --vsync 0|1.",
  fullscreen: "Starts in fullscreen or windowed mode using --fullscreen 0|1.",
  gameDir: "Directory containing default.xex and extracted game archives. Passed as --game-dir.",
  configPath: "Optional TOML settings file. When selected, the launcher passes --config.",
};
const initialHelp = "SELECT A SETTING TO VIEW ITS DESCRIPTION AND ENGINE FLAG.";
const engineBuild = "e6cd339";
const performanceProfiles = [
  { value: "xbox360", label: "Xbox 360 Authentic (Default)", resolution: "1280x720", fps: "30", vsyncCadence: "FIFO", footer: "XBOX 360 AUTHENTIC" },
  { value: "pc1080", label: "PC Standard 1080p", resolution: "1920x1080", fps: "60", vsyncCadence: "Mailbox", footer: "1080P 60FPS" },
  { value: "high1440", label: "High Refresh 1440p", resolution: "2560x1440", fps: "120", vsyncCadence: "Mailbox", footer: "1440P 120FPS" },
  { value: "ultra4k", label: "Unlocked 4K Ultra", resolution: "3840x2160", fps: "0", vsyncCadence: "Immediate", footer: "4K UNLOCKED" },
] as const;
const defaultBindings: Record<string, string> = {
  a: "Button 0",
  b: "Button 1",
  x: "Button 2",
  y: "Button 3",
  lb: "Button 4",
  rb: "Button 5",
  lt: "Button 6",
  rt: "Button 7",
  leftStick: "Axes 0 / 1",
  rightStick: "Axes 2 / 3",
  lsClick: "Button 10",
  rsClick: "Button 11",
  dpadUp: "Button 12",
  dpadRight: "Button 15",
  dpadDown: "Button 13",
  dpadLeft: "Button 14",
  start: "Button 9",
  back: "Button 8",
};
const controllerGroups = [
  { title: "FACE BUTTONS", controls: [["a", "A", "Sprint / Accept"], ["b", "B", "Reload / Back"], ["x", "X", "Jump / Handbrake"], ["y", "Y", "Enter Vehicle"]] },
  { title: "BUMPERS & TRIGGERS", controls: [["lb", "LB", "Weapon Wheel"], ["rb", "RB", "Cover"], ["lt", "LT", "Aim / Brake"], ["rt", "RT", "Fire / Accelerate"]] },
  { title: "STICKS & D-PAD", controls: [["leftStick", "LEFT STICK", "Move / Steer"], ["rightStick", "RIGHT STICK", "Camera / Look"], ["lsClick", "LS CLICK", "Stealth / Horn"], ["rsClick", "RS CLICK", "Look Behind"], ["dpadUp", "D-PAD UP", "Phone"], ["dpadRight", "D-PAD RIGHT", "Headlights / Context"], ["dpadDown", "D-PAD DOWN", "Switch Character"], ["dpadLeft", "D-PAD LEFT", "Radio"]] },
  { title: "SYSTEM", controls: [["start", "START", "Pause Menu"], ["back", "BACK / SELECT", "Camera Mode"]] },
] as const;

function formatBytes(bytes: number) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 ** 2) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
  return `${(bytes / 1024 ** 3).toFixed(2)} GB`;
}

function quote(value: string) {
  return `"${value.replaceAll("\\", "\\\\").replaceAll('"', '\\"')}"`;
}

function SelectControl({
  value,
  onChange,
  options,
  label,
}: {
  value: string;
  onChange: (value: string) => void;
  options: { value: string; label: string }[];
  label: string;
}) {
  return (
    <Select.Root value={value} onValueChange={onChange}>
      <Select.Trigger className="select-trigger" aria-label={label}>
        <Select.Value />
        <Select.Icon><ChevronDown size={14} /></Select.Icon>
      </Select.Trigger>
      <Select.Portal>
        <Select.Content className="select-content" position="popper" sideOffset={4}>
          <Select.ScrollUpButton className="select-scroll"><ChevronUp size={14} /></Select.ScrollUpButton>
          <Select.Viewport>
            {options.map((option) => (
              <Select.Item className="select-item" key={option.value} value={option.value}>
                <Select.ItemText>{option.label}</Select.ItemText>
                <Select.ItemIndicator><Check size={14} /></Select.ItemIndicator>
              </Select.Item>
            ))}
          </Select.Viewport>
          <Select.ScrollDownButton className="select-scroll"><ChevronDown size={14} /></Select.ScrollDownButton>
        </Select.Content>
      </Select.Portal>
    </Select.Root>
  );
}

function Toggle({ checked, onCheckedChange, label }: {
  checked: boolean;
  onCheckedChange: (checked: boolean) => void;
  label: string;
}) {
  return (
    <div className={`toggle-control ${checked ? "is-on" : ""}`}>
      <span className={!checked ? "toggle-selected" : ""}>OFF</span>
      <Switch.Root className="switch-root" checked={checked} onCheckedChange={onCheckedChange} aria-label={label}>
        <Switch.Thumb className="switch-thumb" />
      </Switch.Root>
      <span className={checked ? "toggle-selected" : ""}>ON</span>
    </div>
  );
}

function CalibrationSlider({ label, value, max, suffix, onChange }: {
  label: string;
  value: number;
  max: number;
  suffix: string;
  onChange: (value: number) => void;
}) {
  return (
    <div className="calibration-slider">
      <div className="calibration-label"><span>{label}</span><b>{value}{suffix}</b></div>
      <Slider.Root className="range-root" value={[value]} max={max} step={1} onValueChange={([next]) => onChange(next)} aria-label={label}>
        <Slider.Track className="range-track"><Slider.Range className="range-fill" /></Slider.Track>
        <Slider.Thumb className="range-thumb" />
      </Slider.Root>
      <div className="range-scale"><span>0{suffix}</span><span>{max}{suffix}</span></div>
    </div>
  );
}

function App() {
  const [savedDashboardProfile] = useState<Record<string, unknown>>(() => {
    try {
      const saved = window.localStorage.getItem("fiveinvader-dashboard-profile");
      if (!saved) return {};
      const parsed: unknown = JSON.parse(saved);
      return parsed && typeof parsed === "object" ? parsed as Record<string, unknown> : {};
    } catch {
      return {};
    }
  });
  const [tab, setTab] = useState<TabId>("launch");
  const [gameDir, setGameDir] = useState("");
  const [enginePath, setEnginePath] = useState("");
  const [configPath, setConfigPath] = useState("");
  const [resolution, setResolution] = useState(() => {
    const saved = savedDashboardProfile.resolution;
    return typeof saved === "string" && ["1280x720", "1920x1080", "2560x1440", "3840x2160"].includes(saved)
      ? saved
      : "1280x720";
  });
  const [fps, setFps] = useState(() => {
    const saved = savedDashboardProfile.fps;
    return typeof saved === "string" && ["0", "30", "60", "120"].includes(saved) ? saved : "30";
  });
  const [language, setLanguage] = useState<Language>("en");
  const [vsync, setVsync] = useState(() => savedDashboardProfile.vsync !== false);
  const [vsyncCadence, setVsyncCadence] = useState(() => {
    const saved = savedDashboardProfile.vsyncCadence;
    if (typeof saved === "string" && ["Mailbox", "FIFO", "Immediate"].includes(saved)) return saved;
    return savedDashboardProfile.vsync === false ? "Immediate" : "FIFO";
  });
  const [fullscreen, setFullscreen] = useState(
    () => typeof savedDashboardProfile.fullscreen === "boolean" ? savedDashboardProfile.fullscreen : false,
  );
  const [quickPrefs, setQuickPrefs] = useState(() => {
    const defaults = { gamepadHooks: true, telemetryHud: true, headless: false };
    return {
      gamepadHooks: typeof savedDashboardProfile.gamepadHooks === "boolean" ? savedDashboardProfile.gamepadHooks : defaults.gamepadHooks,
      telemetryHud: typeof savedDashboardProfile.telemetryHud === "boolean" ? savedDashboardProfile.telemetryHud : defaults.telemetryHud,
      headless: typeof savedDashboardProfile.headless === "boolean" ? savedDashboardProfile.headless : defaults.headless,
    };
  });
  const [enableMangoHud, setEnableMangoHud] = useState(
    () => typeof savedDashboardProfile.enableMangoHud === "boolean" ? savedDashboardProfile.enableMangoHud : false,
  );
  const [inspection, setInspection] = useState<GameInspection | null>(null);
  const [inspectionError, setInspectionError] = useState("");
  const [help, setHelp] = useState(initialHelp);
  const [running, setRunning] = useState(false);
  const [launchError, setLaunchError] = useState("");
  const [logs, setLogs] = useState<{ stream: string; text: string; time: string }[]>([]);
  const [integrityOpen, setIntegrityOpen] = useState(false);
  const [configOpen, setConfigOpen] = useState(false);
  const [configText, setConfigText] = useState("");
  const [configError, setConfigError] = useState("");
  const [configSaved, setConfigSaved] = useState(false);
  const [gamepads, setGamepads] = useState<{ index: number; id: string; buttons: number[]; axes: number[] }[]>([]);
  const [selectedGamepad, setSelectedGamepad] = useState("auto");
  const [listeningControl, setListeningControl] = useState<string | null>(null);
  const [controllerSettings, setControllerSettings] = useState<ControllerSettings>({
    bindings: defaultBindings,
    leftDeadzone: 12,
    rightDeadzone: 12,
    triggerThreshold: 10,
    vibration: true,
    invertLookY: false,
  });
  const [controllerSettingsLoaded, setControllerSettingsLoaded] = useState(false);
  const [controllerSaveState, setControllerSaveState] = useState<"saved" | "saving" | "error">("saved");
  const [controllerSaveError, setControllerSaveError] = useState("");
  const logsEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    window.localStorage.setItem("fiveinvader-dashboard-profile", JSON.stringify({
      ...quickPrefs,
      enableMangoHud,
      resolution,
      fps,
      vsync,
      vsyncCadence,
      fullscreen,
    }));
  }, [enableMangoHud, fps, fullscreen, quickPrefs, resolution, vsync, vsyncCadence]);

  const fpsValue = Number(fps);
  const resolutionBadge = resolution === "3840x2160" ? "4K" : `${resolution.split("x")[1] ?? "720"}P`;
  const activeProfile = performanceProfiles.find((profile) =>
    profile.resolution === resolution && profile.fps === fps && profile.vsyncCadence === vsyncCadence,
  );
  const selectedProfile = activeProfile?.value ?? "custom";
  function applyProfile(value: string) {
    const profile = performanceProfiles.find((candidate) => candidate.value === value);
    if (!profile) return;
    setResolution(profile.resolution);
    setFps(profile.fps);
    setVsyncCadence(profile.vsyncCadence);
    setVsync(profile.vsyncCadence !== "Immediate");
  }
  const command = useMemo(() => {
    if (!gameDir) return "Awaiting game directory…";
    return [
      ...(enableMangoHud ? ["MANGOHUD=1"] : []),
      quote(enginePath || "./FiveInvader"),
      "--game-dir", quote(gameDir),
      ...(configPath ? ["--config", quote(configPath)] : []),
      "--lang", language,
      "--res", resolution,
      "--vsync", vsync ? "1" : "0",
      "--fullscreen", fullscreen ? "1" : "0",
      "--fps", String(fpsValue),
    ].join(" ");
  }, [configPath, enableMangoHud, enginePath, fpsValue, fullscreen, gameDir, language, resolution, vsync]);

  useEffect(() => {
    let active = true;
    void window.fiveinvader.readControllerSettings().then((settings) => {
      if (!active) return;
      setControllerSettings({
        ...settings,
        bindings: { ...defaultBindings, ...settings.bindings },
      });
      setControllerSettingsLoaded(true);
    }).catch((error: Error) => {
      if (!active) return;
      setControllerSaveState("error");
      setControllerSaveError(error.message);
      setControllerSettingsLoaded(true);
    });
    void window.fiveinvader.defaults().then((defaults) => {
      if (!active) return;
      setGameDir(defaults.gameDir);
      setEnginePath(defaults.enginePath);
      setRunning(defaults.running);
      return window.fiveinvader.inspectGame(defaults.gameDir)
        .then((result) => active && setInspection(result))
        .catch((error: Error) => active && setInspectionError(error.message));
    }).catch((error: Error) => active && setLaunchError(error.message));
    const unsubscribeOutput = window.fiveinvader.onOutput((stream, text) => {
      setLogs((current) => [...current.slice(-499), { stream, text, time: new Date().toLocaleTimeString() }]);
    });
    const unsubscribeStatus = window.fiveinvader.onStatus((status) => {
      setRunning(status.running);
      if (!status.running && status.code !== undefined) {
        setLogs((current) => [...current, {
          stream: "system",
          text: `Engine exited with ${status.code === null ? `signal ${status.signal ?? "unknown"}` : `code ${status.code}`}\n`,
          time: new Date().toLocaleTimeString(),
        }]);
      }
    });
    return () => {
      active = false;
      unsubscribeOutput();
      unsubscribeStatus();
    };
  }, []);

  useEffect(() => {
    if (!controllerSettingsLoaded) return;
    setControllerSaveState("saving");
    const timer = window.setTimeout(() => {
      void window.fiveinvader.writeControllerSettings(controllerSettings).then(() => {
        setControllerSaveState("saved");
        setControllerSaveError("");
      }).catch((error: Error) => {
        setControllerSaveState("error");
        setControllerSaveError(error.message);
      });
    }, 350);
    return () => window.clearTimeout(timer);
  }, [controllerSettings, controllerSettingsLoaded]);

  useEffect(() => {
    let frame = 0;
    let hasSampled = false;
    let previousButtons = new Map<string, boolean>();
    let previousAxes = new Map<string, number>();
    const sampleGamepads = () => {
      const current = Array.from(navigator.getGamepads())
        .filter((gamepad): gamepad is Gamepad => gamepad !== null)
        .map((gamepad) => ({
          index: gamepad.index,
          id: gamepad.id,
          buttons: gamepad.buttons.map((button) => button.value),
          axes: gamepad.axes.map((axis) => Math.round(axis * 100) / 100),
        }));
      setGamepads((previous) => {
        const same = previous.length === current.length && previous.every((pad, index) => {
          const next = current[index];
          return pad.index === next.index && pad.id === next.id
            && pad.buttons.length === next.buttons.length
            && pad.buttons.every((pressed, button) => pressed === next.buttons[button])
            && pad.axes.every((axis, axisIndex) => axis === next.axes[axisIndex]);
        });
        return same ? previous : current;
      });

      if (tab === "controller" && listeningControl && hasSampled) {
        let captured: string | null = null;
        for (const pad of current) {
          for (const [button, pressed] of pad.buttons.entries()) {
            const key = `${pad.index}:button:${button}`;
            if (pressed > 0.6 && !previousButtons.get(key)) {
              captured = `PAD ${pad.index} · BUTTON ${button}`;
              break;
            }
          }
          if (captured) break;
          for (const [axis, value] of pad.axes.entries()) {
            const key = `${pad.index}:axis:${axis}`;
            if (Math.abs(value) > 0.65 && Math.abs(previousAxes.get(key) ?? 0) <= 0.65) {
              captured = `PAD ${pad.index} · AXIS ${axis} ${value > 0 ? "+" : "-"}`;
              break;
            }
          }
          if (captured) break;
        }
        if (captured) saveBinding(listeningControl, captured);
      }

      previousButtons = new Map(current.flatMap((pad) =>
        pad.buttons.map((pressed, button) => [`${pad.index}:button:${button}`, pressed > 0.6] as const),
      ));
      previousAxes = new Map(current.flatMap((pad) =>
        pad.axes.map((value, axis) => [`${pad.index}:axis:${axis}`, value] as const),
      ));
      hasSampled = true;
      frame = requestAnimationFrame(sampleGamepads);
    };
    frame = requestAnimationFrame(sampleGamepads);
    return () => cancelAnimationFrame(frame);
  }, [tab, listeningControl]);

  useEffect(() => {
    if (!listeningControl || tab !== "controller") return;
    const captureKey = (event: KeyboardEvent) => {
      if (event.repeat || event.metaKey || event.ctrlKey || event.altKey) return;
      event.preventDefault();
      event.stopPropagation();
      const key = event.key === " " ? "SPACE" : event.key.length === 1 ? event.key.toUpperCase() : event.key.toUpperCase();
      saveBinding(listeningControl, `KEY ${key}`);
    };
    window.addEventListener("keydown", captureKey, true);
    return () => window.removeEventListener("keydown", captureKey, true);
  }, [listeningControl, tab]);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [logs]);

  const settings: LaunchSettings = {
    gameDir,
    ...(configPath ? { configPath } : {}),
    language,
    resolution,
    vsync,
    fullscreen,
    fps: fpsValue,
    enableMangoHud,
  };

  async function inspect(pathValue = gameDir) {
    setInspectionError("");
    try {
      const result = await window.fiveinvader.inspectGame(pathValue);
      setInspection(result);
      setGameDir(pathValue);
    } catch (error) {
      setInspection(null);
      setInspectionError(error instanceof Error ? error.message : String(error));
    }
  }

  async function chooseGameDir() {
    const chosen = await window.fiveinvader.chooseGameDir();
    if (chosen) await inspect(chosen);
  }

  async function chooseConfig() {
    const chosen = await window.fiveinvader.chooseConfig();
    if (chosen) {
      setConfigPath(chosen);
      setConfigError("");
    }
  }

  async function openConfigEditor() {
    setConfigError("");
    setConfigSaved(false);
    try {
      if (!configPath) {
        const chosen = await window.fiveinvader.chooseConfig();
        if (!chosen) return;
        setConfigPath(chosen);
        setConfigText(await window.fiveinvader.readConfig(chosen));
      } else {
        setConfigText(await window.fiveinvader.readConfig(configPath));
      }
      setConfigOpen(true);
    } catch (error) {
      setConfigError(error instanceof Error ? error.message : String(error));
    }
  }

  async function saveConfig() {
    if (!configPath) return;
    setConfigError("");
    try {
      await window.fiveinvader.writeConfig(configPath, configText);
      setConfigSaved(true);
    } catch (error) {
      setConfigError(error instanceof Error ? error.message : String(error));
    }
  }

  async function launchGame() {
    setLaunchError("");
    if (running) {
      await window.fiveinvader.stop();
      return;
    }
    try {
      await window.fiveinvader.launch(settings);
      setRunning(true);
    } catch (error) {
      setLaunchError(error instanceof Error ? error.message : String(error));
    }
  }

  function rowProps(key: SettingKey) {
    return {
      onMouseEnter: () => setHelp(descriptions[key]),
      onFocusCapture: () => setHelp(descriptions[key]),
      onMouseLeave: () => setHelp(initialHelp),
    };
  }

  function saveBinding(control: string, value: string) {
    setControllerSettings((current) => ({
      ...current,
      bindings: { ...current.bindings, [control]: value },
    }));
    setListeningControl(null);
  }

  function updateControllerSetting<K extends keyof ControllerSettings>(key: K, value: ControllerSettings[K]) {
    setControllerSettings((current) => ({ ...current, [key]: value }));
  }

  function resetControllerSettings() {
    setControllerSettings({
      bindings: { ...defaultBindings },
      leftDeadzone: 12,
      rightDeadzone: 12,
      triggerThreshold: 10,
      vibration: true,
      invertLookY: false,
    });
    setListeningControl(null);
  }

  const activeGamepad = gamepads.find((gamepad) => gamepad.index === Number(selectedGamepad)) ?? gamepads[0];
  const activeButtons = activeGamepad?.buttons ?? [];
  const activeAxes = activeGamepad?.axes ?? [];

  const tabs: { value: TabId; label: string }[] = [
    { value: "launch", label: "DASHBOARD" },
    { value: "controller", label: "INPUT & CONTROLS" },
    { value: "system", label: "ROMFS & SHADERS" },
    { value: "graphics", label: "GRAPHICS & GFX" },
    { value: "logs", label: "CONSOLE" },
  ];

  return (
    <Tabs.Root
        value={tab}
        onValueChange={(value) => setTab(value as TabId)}
        className="launcher-shell"
      >
        <div className="strobe-field" />
        <header className="top-chrome">
          <div className="brand-lockup" aria-label="FiveInvader">
            <img src="/Untitled.png" alt="FiveInvader" />
          </div>
          <Tabs.List className="menu-tabs" aria-label="Launcher sections">
            {tabs.map((item) => (
              <Tabs.Trigger
                className="menu-tab"
                key={item.value}
                value={item.value}
                onMouseEnter={() => setHelp(initialHelp)}
              >
                {item.label}
              </Tabs.Trigger>
            ))}
          </Tabs.List>
          <div className="header-right">
            <div className={`engine-status ${running ? "engine-running" : ""}`}>
              <span />
              {running ? "ENGINE RUNNING" : "ENGINE IDLE"}
            </div>
          <div className="window-actions">
            <button aria-label="Minimize" onClick={() => void window.fiveinvader.windowAction("minimize")}><Minus size={13} /></button>
            <button aria-label="Close" onClick={() => void window.fiveinvader.windowAction("close")}><X size={14} /></button>
          </div>
          </div>
        </header>

        <main className="content-stage w-full min-w-0">
            <Tabs.Content value="launch" className="tab-content dashboard-view w-full min-w-0 flex-1 overflow-hidden outline-none data-[state=inactive]:hidden">
              <div className="dashboard-grid">
                <section className="dashboard-card hero-card">
                  <div className="hero-card-content">
                    <div className="hero-tags">
                      <span className="hero-tag tag-green">GTA V 1.0.0 • XENON PPC RECOMP</span>
                      <span className="hero-tag tag-blue">VULKAN 1.3 NATIVE</span>
                      <span className="hero-tag tag-red">DISC 2 / BUILD {engineBuild.toUpperCase()}</span>
                    </div>
                    <div className="hero-headline">
                      <h1>GRAND THEFT AUTO V</h1>
                      <h2>FIVEINVADER RUNTIME</h2>
                    </div>
                    <div className="hero-rule" />
                    <p className="hero-description">
                      Native PowerPC statically recompiled binary targeting host x86_64 CPU instructions and Vulkan compute/raster pipelines. Direct 32-bit virtual file system memory mapping with native input hooks and multi-threaded RAGE scheduler.
                    </p>
                    {inspectionError && <p className="error-line"><AlertTriangle size={13} /> {inspectionError}</p>}
                    {launchError && <p className="error-line"><AlertTriangle size={13} /> {launchError}</p>}
                  </div>
                  <button
                    className={`dashboard-launch ${running ? "stop-button" : ""}`}
                    onClick={() => void launchGame()}
                    disabled={!running && !inspection?.defaultXexPresent}
                  >
                    {running ? <Square size={21} fill="currentColor" /> : <Play size={22} fill="currentColor" />}
                    <span>{running ? "STOP ENGINE PROCESS" : "LAUNCH FIVEINVADER"}</span>
                  </button>
                </section>

                <section className="dashboard-card quick-config-card">
                  <div className="quick-config-main">
                    <div className="quick-config-heading">
                      <span><Settings2 size={17} /> QUICK CONFIGURATION</span>
                      <small>PROFILE AUTO-SAVED</small>
                    </div>

                    <div className="quick-config-fields">
                      <label className="quick-config-field">
                        <span>PRESET PROFILE</span>
                        <SelectControl
                          label="Preset profile"
                          value={selectedProfile}
                          onChange={applyProfile}
                          options={[
                            ...performanceProfiles.map(({ value, label }) => ({ value, label })),
                            { value: "custom", label: "Custom" },
                          ]}
                        />
                      </label>
                      <label className="quick-config-field">
                        <span>RENDER TARGET RESOLUTION</span>
                        <SelectControl
                          label="Render target resolution"
                          value={resolution}
                          onChange={setResolution}
                          options={[
                            { value: "1280x720", label: "720p (1280x720)" },
                            { value: "1920x1080", label: "1080p (Native) - 1920x1080" },
                            { value: "2560x1440", label: "1440p (2560x1440)" },
                            { value: "3840x2160", label: "4K (3840x2160)" },
                          ]}
                        />
                      </label>

                      <label className="quick-config-field">
                        <span>V-SYNC PRESENTATION CADENCE</span>
                        <SelectControl
                          label="V-Sync presentation cadence"
                          value={vsyncCadence}
                          onChange={(value) => {
                            setVsyncCadence(value);
                            setVsync(value !== "Immediate");
                          }}
                          options={[
                            { value: "Mailbox", label: "Mailbox (Low-latency)" },
                            { value: "FIFO", label: "FIFO (Strict VSync)" },
                            { value: "Immediate", label: "Immediate (Unlocked)" },
                          ]}
                        />
                      </label>

                      <div className="quick-config-toggle">
                        <span>NATIVE GAMEPAD HOOKS</span>
                        <Toggle
                          label="Native gamepad hooks"
                          checked={quickPrefs.gamepadHooks}
                          onCheckedChange={(gamepadHooks) => setQuickPrefs((prefs) => ({ ...prefs, gamepadHooks }))}
                        />
                      </div>
                      <div className="quick-config-toggle">
                        <span>ON-SCREEN TELEMETRY HUD</span>
                        <Toggle
                          label="On-screen telemetry HUD"
                          checked={quickPrefs.telemetryHud}
                          onCheckedChange={(telemetryHud) => setQuickPrefs((prefs) => ({ ...prefs, telemetryHud }))}
                        />
                      </div>
                      <div className="quick-config-toggle quick-config-toggle-description">
                        <div>
                          <span>MANGOHUD PERFORMANCE OVERLAY</span>
                          <small>INJECT MANGOHUD=1 ENVIRONMENT VARIABLE ON LAUNCH</small>
                        </div>
                        <Toggle
                          label="MangoHud performance overlay"
                          checked={enableMangoHud}
                          onCheckedChange={setEnableMangoHud}
                        />
                      </div>
                      <div className="quick-config-toggle">
                        <span>HEADLESS EXECUTION (CI/TESTS)</span>
                        <Toggle
                          label="Headless execution"
                          checked={quickPrefs.headless}
                          onCheckedChange={(headless) => setQuickPrefs((prefs) => ({ ...prefs, headless }))}
                        />
                      </div>
                    </div>
                  </div>

                  <div className="quick-config-footer">
                    <span>HOST: X86_64 LINUX</span>
                    <span>VULKAN 1.3 + AMD RADEON 680M</span>
                  </div>
                </section>
              </div>
            </Tabs.Content>

            <Tabs.Content value="graphics" className="tab-content settings-view w-full min-w-0 flex-1 overflow-hidden outline-none data-[state=inactive]:hidden">
              <div className="settings-content">
                <div className="section-heading">
                  <div><span className="eyebrow">DISPLAY CONFIGURATION</span><h2 className="font-heading tracking-wider uppercase">GRAPHICS & DISPLAY</h2></div>
                  <span className="section-index">01 / 03</span>
                </div>
                <div className="settings-columns">
                <div className="settings-table settings-list">
                  <div className="table-caption">VIDEO OUTPUT <span>ENGINE OVERRIDES</span></div>
                  <div className="setting-row" {...rowProps("resolution")}>
                    <div className="setting-label"><Maximize2 size={15} /><div><b>Resolution</b><small>RENDER WINDOW SIZE</small></div></div>
                    <SelectControl label="Resolution" value={resolution} onChange={setResolution}
                      options={["1280x720", "1920x1080", "2560x1440", "3840x2160"].map((value) => ({ value, label: value }))} />
                  </div>
                  <div className="setting-row" {...rowProps("fps")}>
                    <div className="setting-label"><Radio size={15} /><div><b>Frame Rate Limit</b><small>FRAME PACING TARGET</small></div></div>
                    <SelectControl label="Frame rate limit" value={fps} onChange={setFps}
                      options={[{ value: "30", label: "30 FPS" }, { value: "60", label: "60 FPS" }, { value: "120", label: "120 FPS" }, { value: "0", label: "UNLIMITED" }]} />
                  </div>
                  <div className="setting-row" {...rowProps("vsync")}>
                    <div className="setting-label"><Square size={15} /><div><b>Vertical Sync</b><small>DISPLAY SYNCHRONIZATION</small></div></div>
                    <Toggle label="Vertical sync" checked={vsync} onCheckedChange={(checked) => {
                      setVsync(checked);
                      setVsyncCadence(checked ? "FIFO" : "Immediate");
                    }} />
                  </div>
                  <div className="setting-row" {...rowProps("fullscreen")}>
                    <div className="setting-label"><Maximize2 size={15} /><div><b>Display Mode</b><small>WINDOW PRESENTATION</small></div></div>
                    <Toggle label="Fullscreen" checked={fullscreen} onCheckedChange={setFullscreen} />
                  </div>
                  <div className="setting-row" {...rowProps("language")}>
                    <div className="setting-label"><Gamepad2 size={15} /><div><b>Game Language</b><small>ENGINE LOCALIZATION</small></div></div>
                    <SelectControl label="Game language" value={language} onChange={(value) => setLanguage(value as Language)}
                      options={Object.entries(languageNames).map(([value, label]) => ({ value, label }))} />
                  </div>
                </div>
                <aside className="side-readout settings-summary">
                  <div className="readout-title"><span>LIVE PREVIEW</span><span className="live-dot" /></div>
                  <div className="display-preview"><div className="preview-grid"><span>FIVE</span><b>INVADER</b><small>{resolution}</small></div></div>
                  <dl className="summary-list">
                    <div><dt>OUTPUT</dt><dd>{fullscreen ? "FULLSCREEN" : "WINDOWED"}</dd></div>
                    <div><dt>VSYNC</dt><dd>{vsync ? "ENABLED" : "DISABLED"}</dd></div>
                    <div><dt>FRAME TARGET</dt><dd>{fpsValue === 0 ? "UNLIMITED" : `${fpsValue} FPS`}</dd></div>
                    <div><dt>LANGUAGE</dt><dd>{languageNames[language].toUpperCase()}</dd></div>
                  </dl>
                </aside>
                </div>
              </div>
            </Tabs.Content>

            <Tabs.Content value="controller" className="tab-content settings-view w-full min-w-0 flex-1 overflow-hidden outline-none data-[state=inactive]:hidden">
              <div className="settings-content controller-content">
                <div className="section-heading">
                  <div><span className="eyebrow">DEVICE INPUT & MAPPING</span><h2 className="font-heading tracking-wider uppercase">CONTROLLER & INPUT</h2></div>
                  <span className="section-index">02 / 03</span>
                </div>
                <div className="gamepad-detection">
                  <div className="gamepad-state">
                    <span className={`status-pip ${gamepads.length ? "pip-good" : "pip-red"}`} />
                    <b>{activeGamepad
                      ? `XINPUT CONTROLLER ${activeGamepad.index}: CONNECTED`
                      : "NO GAMEPAD DETECTED — KEYBOARD FALLBACK ACTIVE"}</b>
                    {activeGamepad && <small title={activeGamepad.id}>{activeGamepad.id}</small>}
                  </div>
                  <SelectControl
                    label="Controller device"
                    value={activeGamepad ? String(activeGamepad.index) : "none"}
                    onChange={(value) => setSelectedGamepad(value === "none" ? "auto" : value)}
                    options={gamepads.length
                      ? gamepads.map((gamepad) => ({ value: String(gamepad.index), label: `CONTROLLER ${gamepad.index}` }))
                      : [{ value: "none", label: "NO DEVICE" }]}
                  />
                </div>
                <div className="settings-columns controller-columns">
                  <div className="settings-table settings-list binding-list">
                    <div className="table-caption"><span>CONTROL MAP</span><span>{controllerSaveState === "saving" ? "SAVING…" : controllerSaveState === "error" ? "SAVE ERROR" : "LAUNCHER CONFIG SAVED"}</span></div>
                    {controllerGroups.map((group) => (
                      <section className="binding-group" key={group.title}>
                        <h3>{group.title}</h3>
                        {group.controls.map(([id, name, action]) => (
                          <div className="binding-row" key={id}>
                            <div className="binding-name"><b>{name}</b><small>{action}</small></div>
                            <button
                              className={`binding-button ${listeningControl === id ? "binding-listening" : ""}`}
                              onClick={() => setListeningControl(listeningControl === id ? null : id)}
                            >
                              {listeningControl === id ? "PRESS ANY BUTTON OR KEY..." : controllerSettings.bindings[id] ?? "UNBOUND"}
                            </button>
                          </div>
                        ))}
                      </section>
                    ))}
                    {controllerSaveState === "error" && <p className="error-line"><AlertTriangle size={13} /> {controllerSaveError}</p>}
                  </div>
                  <aside className="side-readout settings-summary calibration-card">
                    <div className="readout-title"><span>LIVE INPUT / CALIBRATION</span><SlidersHorizontal size={13} /></div>
                    {activeGamepad ? (
                      <div className="stick-monitor">
                        {[
                          { title: "LEFT STICK", x: activeAxes[0] ?? 0, y: activeAxes[1] ?? 0 },
                          { title: "RIGHT STICK", x: activeAxes[2] ?? 0, y: activeAxes[3] ?? 0 },
                        ].map((stick) => (
                          <div className="stick-readout" key={stick.title}>
                            <div className="stick-readout-title"><b>{stick.title}</b><span>X {stick.x.toFixed(2)} / Y {stick.y.toFixed(2)}</span></div>
                            <div className="stick-crosshair"><i /><span style={{ left: `${50 + stick.x * 43}%`, top: `${50 + stick.y * 43}%` }} /></div>
                          </div>
                        ))}
                        <div className="trigger-readouts">
                          {[                          { label: "LT", value: activeButtons[6] ?? 0 }, { label: "RT", value: activeButtons[7] ?? 0 }].map((trigger) => (
                            <div className="trigger-readout" key={trigger.label}>
                              <div><b>{trigger.label}</b><span>{Math.round(trigger.value * 100)}%</span></div>
                              <div className="trigger-track"><span style={{ width: `${Math.min(100, trigger.value * 100)}%` }} /></div>
                            </div>
                          ))}
                        </div>
                      </div>
                    ) : (
                      <div className="no-input"><Gamepad2 size={22} /><span>CONNECT A GAMEPAD TO VIEW LIVE INPUT</span></div>
                    )}
                    <div className="calibration-controls">
                      <CalibrationSlider label="LEFT STICK DEADZONE" value={controllerSettings.leftDeadzone} max={40} suffix="%" onChange={(value) => updateControllerSetting("leftDeadzone", value)} />
                      <CalibrationSlider label="RIGHT STICK DEADZONE" value={controllerSettings.rightDeadzone} max={40} suffix="%" onChange={(value) => updateControllerSetting("rightDeadzone", value)} />
                      <CalibrationSlider label="TRIGGER THRESHOLD" value={controllerSettings.triggerThreshold} max={100} suffix="%" onChange={(value) => updateControllerSetting("triggerThreshold", value)} />
                    </div>
                    <div className="controller-toggles">
                      <div><span>VIBRATION / RUMBLE</span><Toggle label="Vibration and rumble" checked={controllerSettings.vibration} onCheckedChange={(value) => updateControllerSetting("vibration", value)} /></div>
                      <div><span>INVERT LOOK Y-AXIS</span><Toggle label="Invert look Y axis" checked={controllerSettings.invertLookY} onCheckedChange={(value) => updateControllerSetting("invertLookY", value)} /></div>
                    </div>
                    <button className="reset-controller" onClick={resetControllerSettings}><RotateCcw size={13} /> RESET TO DEFAULT XBOX 360 LAYOUT</button>
                  </aside>
                </div>
              </div>
            </Tabs.Content>

            <Tabs.Content value="system" className="tab-content settings-view w-full min-w-0 flex-1 overflow-hidden outline-none data-[state=inactive]:hidden">
              <div className="settings-content">
                <div className="section-heading">
                  <div><span className="eyebrow">GAME DATA & CONFIGURATION</span><h2 className="font-heading tracking-wider uppercase">ROMFS & SHADERS</h2></div>
                  <span className="section-index">03 / 03</span>
                </div>
                <div className="settings-columns">
                <div className="settings-table settings-list">
                  <div className="table-caption">LOCAL INSTALLATION <span>FILESYSTEM</span></div>
                  <div className="setting-row path-row" {...rowProps("gameDir")}>
                    <div className="setting-label"><HardDrive size={15} /><div><b>Game Directory</b><small>EXTRACTED DISC 2 ROOT</small></div></div>
                    <div className="path-value" title={gameDir}>{gameDir}</div>
                    <button className="icon-action" onClick={() => void chooseGameDir()} aria-label="Browse game directory"><FolderOpen size={16} /></button>
                  </div>
                  <div className="setting-row path-row" {...rowProps("configPath")}>
                    <div className="setting-label"><FileCode2 size={15} /><div><b>Custom Config</b><small>OPTIONAL TOML OVERRIDE</small></div></div>
                    <div className={`path-value ${!configPath ? "muted" : ""}`} title={configPath}>{configPath || "NOT SET — ENGINE DEFAULTS"}</div>
                    <button className="icon-action" onClick={() => void chooseConfig()} aria-label="Browse config file"><FolderOpen size={16} /></button>
                  </div>
                  <div className="system-actions">
                    <button className="outline-action" onClick={() => void inspect()}><ShieldCheck size={14} /> SCAN GAME FILES</button>
                    <button className="outline-action" onClick={() => void openConfigEditor()}><FileCode2 size={14} /> EDIT CONFIG.TOML</button>
                  </div>
                  {inspectionError && <p className="error-line"><AlertTriangle size={13} /> {inspectionError}</p>}
                  <div className="path-footnote">The game directory must contain <code>default.xex</code>. RPF files are enumerated, not modified.</div>
                </div>
                <aside className="side-readout settings-summary system-summary">
                  <div className="readout-title"><span>INSTALLATION STATUS</span><span className={`live-dot ${inspection?.defaultXexPresent ? "" : "dot-red"}`} /></div>
                  <div className="install-status">
                    <div className={inspection?.defaultXexPresent ? "status-good" : "status-bad"}>
                      {inspection?.defaultXexPresent ? <ShieldCheck size={28} /> : <AlertTriangle size={28} />}
                    </div>
                    <strong>{inspection?.defaultXexPresent ? "GAME DATA DETECTED" : "GAME DATA NOT FOUND"}</strong>
                    <small>{inspection?.defaultXexPresent ? "DISC 2 ROOT IS READABLE" : "SELECT THE EXTRACTED DISC 2 FOLDER"}</small>
                  </div>
                  <dl className="summary-list">
                    <div><dt>DEFAULT.XEX</dt><dd>{inspection?.defaultXexPresent ? formatBytes(inspection.defaultXexBytes) : "MISSING"}</dd></div>
                    <div><dt>RPF ARCHIVES</dt><dd>{inspection?.archiveCount ?? "—"} FOUND</dd></div>
                    <div><dt>ARCHIVE SIZE</dt><dd>{inspection ? formatBytes(inspection.totalArchiveBytes) : "—"}</dd></div>
                    <div><dt>CONFIG</dt><dd>{configPath ? "CUSTOM" : "DEFAULT"}</dd></div>
                  </dl>
                  <button className="text-action" onClick={() => setIntegrityOpen(true)}>VIEW FILE MANIFEST <span>↗</span></button>
                </aside>
                </div>
              </div>
            </Tabs.Content>

            <Tabs.Content value="logs" className="tab-content log-view w-full min-w-0 flex-1 overflow-hidden outline-none data-[state=inactive]:hidden">
              <div className="section-heading">
                <div><span className="eyebrow">NATIVE ENGINE PROCESS</span><h2 className="font-heading tracking-wider uppercase">LOG CONSOLE</h2></div>
                <div className={`process-status ${running ? "process-running" : ""}`}><span /> {running ? "PROCESS ACTIVE" : "STANDBY"}</div>
              </div>
              <div className="terminal-frame">
                <div className="terminal-top"><span><Terminal size={14} /> STDOUT / STDERR</span><span>{logs.length.toString().padStart(3, "0")} EVENTS</span></div>
                <div className="terminal-lines">
                  {logs.length === 0 ? (
                    <div className="terminal-empty"><Terminal size={22} /><span>ENGINE OUTPUT WILL APPEAR HERE</span><small>Launch the game to begin streaming native process logs.</small></div>
                  ) : logs.map((entry, index) => (
                    <div className={`terminal-line stream-${entry.stream}`} key={`${entry.time}-${index}`}>
                      <time>{entry.time}</time><b>{entry.stream.toUpperCase()}</b><pre>{entry.text}</pre>
                    </div>
                  ))}
                  <div ref={logsEndRef} />
                </div>
                <div className="terminal-command"><span>&gt;_</span><code>{command}</code></div>
              </div>
            </Tabs.Content>
        </main>

        <footer className="action-bar">
          <div className="telemetry-group">
            <div className="telemetry-item">
              <b>RPF ARCHIVES:</b>
              <span className={`telemetry-pill ${inspection?.archiveCount ? "telemetry-good" : "telemetry-offline"}`}>
                {inspection?.archiveCount ? `${inspection.archiveCount} FOUND` : "NONE FOUND"}
              </span>
              <span className="telemetry-detail" title={gameDir}>
                {gameDir ? gameDir.split(/[\\/]/).slice(-2).join("/") : "PATH NOT SELECTED"}
              </span>
            </div>
            <div className="telemetry-item">
              <b>INPUT:</b>
              <span className="telemetry-pill telemetry-blue">{activeGamepad ? `CONTROLLER ${activeGamepad.index}` : "KEYBOARD"}</span>
              <span className="telemetry-detail">XBOX 360 LAYOUT</span>
            </div>
          </div>
          <div className="telemetry-group telemetry-group-right">
            <div className="telemetry-item">
              <b>PROFILE:</b>
              <span className="telemetry-pill telemetry-cyan">
                {activeProfile?.footer ?? `${resolutionBadge} ${fpsValue === 0 ? "UNLIMITED" : `${fpsValue}FPS`}`}
              </span>
            </div>
            <div className="telemetry-host">
              <span>HOST: X86_64 POSIX</span><i>•</i><span>PID: --</span>
            </div>
          </div>
        </footer>

        <Dialog.Root open={integrityOpen} onOpenChange={setIntegrityOpen}>
          <Dialog.Portal>
            <Dialog.Overlay className="dialog-overlay" />
            <Dialog.Content className="dialog-panel">
              <div className="dialog-head"><div><span className="eyebrow">DISC 2 / FILE MANIFEST</span><Dialog.Title>GAME DATA SCAN</Dialog.Title></div><Dialog.Close className="icon-action" aria-label="Close"><X size={16} /></Dialog.Close></div>
              <Dialog.Description className="dialog-note">
                Presence and file-size scan only. No known-good hashes are configured, so this does not certify archive integrity.
              </Dialog.Description>
              <div className="manifest-list">
                <div className="manifest-entry">
                  <span className={inspection?.defaultXexPresent ? "status-good" : "status-bad"}>{inspection?.defaultXexPresent ? "FOUND" : "MISSING"}</span>
                  <b>default.xex</b><small>{inspection?.defaultXexPresent ? formatBytes(inspection.defaultXexBytes) : "Required engine entry point"}</small>
                </div>
                {inspection?.archives.map((archive) => (
                  <div className="manifest-entry" key={archive.name}><span className="status-good">FOUND</span><b>{archive.name}</b><small>{formatBytes(archive.bytes)}</small></div>
                ))}
                {!inspection?.archives.length && <div className="manifest-empty">No root-level RPF archives found.</div>}
              </div>
              <div className="dialog-foot"><span>{inspection?.archiveCount ?? 0} RPF ARCHIVES</span><button className="outline-action" onClick={() => void inspect()}>RESCAN</button></div>
            </Dialog.Content>
          </Dialog.Portal>
        </Dialog.Root>

        <Dialog.Root open={configOpen} onOpenChange={setConfigOpen}>
          <Dialog.Portal>
            <Dialog.Overlay className="dialog-overlay" />
            <Dialog.Content className="dialog-panel config-dialog">
              <div className="dialog-head"><div><span className="eyebrow">RAW ENGINE CONFIGURATION</span><Dialog.Title>EDIT CONFIG.TOML</Dialog.Title></div><Dialog.Close className="icon-action" aria-label="Close"><X size={16} /></Dialog.Close></div>
              <Dialog.Description className="dialog-note">{configPath || "Select a TOML file to edit."}</Dialog.Description>
              <textarea className="config-editor" value={configText} onChange={(event) => { setConfigText(event.target.value); setConfigSaved(false); }} spellCheck={false} aria-label="Raw config.toml contents" />
              {configError && <p className="error-line"><AlertTriangle size={13} /> {configError}</p>}
              {configSaved && <p className="saved-line"><Check size={13} /> CONFIGURATION SAVED</p>}
              <div className="dialog-foot"><button className="outline-action" onClick={() => void chooseConfig()}>CHANGE FILE</button><button className="outline-action save-config" onClick={() => void saveConfig()}>SAVE TOML</button></div>
            </Dialog.Content>
          </Dialog.Portal>
        </Dialog.Root>
    </Tabs.Root>
  );
}

export default App;
