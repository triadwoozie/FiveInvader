import { app, BrowserWindow, dialog, ipcMain } from "electron";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { createWriteStream, type WriteStream } from "node:fs";
import { access, mkdir, readFile, readdir, stat, writeFile } from "node:fs/promises";
import { constants } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const workspaceRoot = path.resolve(app.getAppPath(), "..");
const defaultGameDir = path.resolve(workspaceRoot, "rom_data/extracted_disc2");
const defaultEnginePath = path.resolve(workspaceRoot, "FiveInvader/build/FiveInvader/FiveInvader");
const controllerSettingsPath = path.join(app.getPath("userData"), "controller-settings.json");
const languages = new Set(["en", "ja", "de", "fr", "es", "it"]);
const resolutions = new Set(["1280x720", "1920x1080", "2560x1440"]);
const fpsLimits = new Set([0, 30, 60]);
const vsyncModes = new Set(["fifo", "mailbox", "immediate"]);
const windowGates = new Set(["render", "immediate"]);
const runTimeouts = new Set([0, 20, 30, 60]);
const defaultControllerSettings = {
  bindings: {} as Record<string, string>,
  leftDeadzone: 12,
  rightDeadzone: 12,
  triggerThreshold: 10,
  vibration: true,
  invertLookY: false,
};
let mainWindow: BrowserWindow | null = null;
let engineProcess: ChildProcessWithoutNullStreams | null = null;
let engineTimeout: NodeJS.Timeout | null = null;
let runLog: WriteStream | null = null;

function isControllerSettings(value: unknown): value is typeof defaultControllerSettings {
  if (!value || typeof value !== "object") return false;
  const settings = value as Record<string, unknown>;
  if (!settings.bindings || typeof settings.bindings !== "object" || Array.isArray(settings.bindings)) return false;
  if (!Object.entries(settings.bindings).every(([key, binding]) =>
    /^[a-z][a-zA-Z0-9]*$/.test(key) && typeof binding === "string" && binding.length <= 80,
  )) return false;
  return Number.isInteger(settings.leftDeadzone) && Number(settings.leftDeadzone) >= 0 && Number(settings.leftDeadzone) <= 40
    && Number.isInteger(settings.rightDeadzone) && Number(settings.rightDeadzone) >= 0 && Number(settings.rightDeadzone) <= 40
    && Number.isInteger(settings.triggerThreshold) && Number(settings.triggerThreshold) >= 0 && Number(settings.triggerThreshold) <= 100
    && typeof settings.vibration === "boolean" && typeof settings.invertLookY === "boolean";
}

function send(channel: string, ...args: unknown[]) {
  if (mainWindow && !mainWindow.isDestroyed()) mainWindow.webContents.send(channel, ...args);
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1100,
    height: 720,
    title: "FiveInvader",
    minWidth: 980,
    minHeight: 640,
    backgroundColor: "#07080c",
    frame: false,
    show: false,
    webPreferences: {
      preload: path.join(currentDir, "../preload/index.js"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  mainWindow.once("ready-to-show", () => mainWindow?.show());
  if (process.env.ELECTRON_RENDERER_URL) {
    void mainWindow.loadURL(process.env.ELECTRON_RENDERER_URL);
  } else {
    void mainWindow.loadFile(path.join(currentDir, "../renderer/index.html"));
  }
  mainWindow.on("closed", () => {
    mainWindow = null;
  });
}

app.whenReady().then(() => {
  ipcMain.handle("launcher:defaults", () => ({
    gameDir: process.env.FIVEINVADER_GAME_DIR || defaultGameDir,
    enginePath: process.env.FIVEINVADER_ENGINE_PATH || defaultEnginePath,
    running: engineProcess !== null,
  }));
  ipcMain.handle("launcher:choose-game-dir", async () => {
    const result = await dialog.showOpenDialog(mainWindow!, {
      title: "Select extracted Disc 2 directory",
      properties: ["openDirectory"],
    });
    ipcMain.handle("launcher:choose-stream-dir", async () => {
      const result = await dialog.showOpenDialog(mainWindow!, {
        title: "Select optional secondary streaming directory",
        properties: ["openDirectory"],
      });
      return result.canceled ? null : result.filePaths[0];
    });
    return result.canceled ? null : result.filePaths[0];
  });
  ipcMain.handle("launcher:choose-config", async () => {
    const result = await dialog.showOpenDialog(mainWindow!, {
      title: "Select config.toml",
      properties: ["openFile"],
      filters: [{ name: "TOML configuration", extensions: ["toml"] }],
    });
    return result.canceled ? null : result.filePaths[0];
  });
  ipcMain.handle("launcher:read-config", async (_event, configPath: string) => {
    if (typeof configPath !== "string" || !path.isAbsolute(configPath) || path.extname(configPath) !== ".toml") {
      throw new Error("Choose a TOML configuration file.");
    }
    return readFile(configPath, "utf8");
  });
  ipcMain.handle("launcher:write-config", async (_event, configPath: string, contents: string) => {
    if (typeof configPath !== "string" || !path.isAbsolute(configPath) || path.extname(configPath) !== ".toml") {
      throw new Error("Choose a TOML configuration file.");
    }
    if (typeof contents !== "string" || contents.length > 1_000_000) {
      throw new Error("Configuration is invalid or exceeds 1 MB.");
    }
    await writeFile(configPath, contents, "utf8");
    return true;
  });
  ipcMain.handle("launcher:read-controller-settings", async () => {
    try {
      const contents = await readFile(controllerSettingsPath, "utf8");
      const settings: unknown = JSON.parse(contents);
      if (!isControllerSettings(settings)) throw new Error("Saved controller settings are invalid.");
      return settings;
    } catch (error) {
      if (error && typeof error === "object" && "code" in error && error.code === "ENOENT") {
        return defaultControllerSettings;
      }
      throw error;
    }
  });
  ipcMain.handle("launcher:write-controller-settings", async (_event, settings: unknown) => {
    if (!isControllerSettings(settings)) throw new Error("Controller settings failed validation.");
    await writeFile(controllerSettingsPath, `${JSON.stringify(settings, null, 2)}\n`, "utf8");
    return true;
  });
  ipcMain.handle("launcher:inspect-game", async (_event, gameDir: string) => {
    if (typeof gameDir !== "string" || !path.isAbsolute(gameDir)) {
      throw new Error("Choose a valid absolute game directory.");
    }
    const entries = await readdir(gameDir, { withFileTypes: true });
    const xexPath = path.join(gameDir, "default.xex");
    const xexExists = entries.some((entry) => entry.isFile() && entry.name.toLowerCase() === "default.xex");
    const xex = xexExists ? await stat(xexPath) : null;
    if (xex?.isFile()) await access(xexPath, constants.R_OK);
    const archives = await Promise.all(
      entries
        .filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith(".rpf"))
        .map(async (entry) => {
          const file = await stat(path.join(gameDir, entry.name));
          return { name: entry.name, bytes: file.size };
        }),
    );
    return {
      defaultXexPresent: Boolean(xex?.isFile()),
      defaultXexBytes: xex?.size ?? 0,
      archiveCount: archives.length,
      archives: archives.sort((a, b) => a.name.localeCompare(b.name)),
      totalArchiveBytes: archives.reduce((total, archive) => total + archive.bytes, 0),
    };
  });
  ipcMain.handle("launcher:window-action", (_event, action: "minimize" | "close") => {
    if (action === "minimize") mainWindow?.minimize();
    if (action === "close") mainWindow?.close();
  });
  ipcMain.handle("engine:launch", async (_event, settings: {
    gameDir: string;
    streamDir?: string;
    configPath?: string;
    language: string;
    resolution: string;
    vsyncMode: string;
    fullscreen: boolean;
    fps: number;
    enableMangoHud: boolean;
    enableVulkanValidation: boolean;
    verboseAlloc: boolean;
    logVfs: boolean;
    windowGate: string;
    timeoutSeconds: number;
    saveRunLog: boolean;
  }) => {
    if (engineProcess) throw new Error("FiveInvader is already running.");
    if (!settings || !path.isAbsolute(settings.gameDir) || !languages.has(settings.language)) {
      throw new Error("Invalid launcher settings.");
    }
    if (!resolutions.has(settings.resolution) || !fpsLimits.has(settings.fps)
      || !vsyncModes.has(settings.vsyncMode) || !windowGates.has(settings.windowGate)
      || !runTimeouts.has(settings.timeoutSeconds)) {
      throw new Error("Invalid resolution or FPS target.");
    }
    if (typeof settings.fullscreen !== "boolean" || typeof settings.enableMangoHud !== "boolean"
      || typeof settings.enableVulkanValidation !== "boolean" || typeof settings.verboseAlloc !== "boolean"
      || typeof settings.logVfs !== "boolean" || typeof settings.saveRunLog !== "boolean") {
      throw new Error("Invalid display settings.");
    }
    if (settings.streamDir && !path.isAbsolute(settings.streamDir)) {
      throw new Error("The streaming directory must be an absolute path.");
    }
    if (settings.configPath && !path.isAbsolute(settings.configPath)) {
      throw new Error("The config path must be absolute.");
    }
    const enginePath = process.env.FIVEINVADER_ENGINE_PATH || defaultEnginePath;
    await access(enginePath, constants.X_OK);
    await access(path.join(settings.gameDir, "default.xex"), constants.R_OK);
    if (settings.streamDir) {
      const streamInfo = await stat(settings.streamDir);
      if (!streamInfo.isDirectory()) throw new Error("The streaming path must be a directory.");
    }
    if (settings.configPath) await access(settings.configPath, constants.R_OK);

    const args = [
      "--game-dir", settings.gameDir,
      ...(settings.streamDir ? ["--stream-dir", settings.streamDir] : []),
      ...(settings.configPath ? ["--config", settings.configPath] : []),
      "--lang", settings.language,
      "--res", settings.resolution,
      "--vsync-mode", settings.vsyncMode,
      "--window-gate", settings.windowGate,
      "--fullscreen", settings.fullscreen ? "1" : "0",
      "--fps", String(settings.fps),
      ...(settings.verboseAlloc ? ["--verbose-alloc"] : []),
      ...(settings.logVfs ? ["--log-vfs"] : []),
    ];
    const command = [
      ...(settings.enableMangoHud ? ["MANGOHUD=1"] : []),
      ...(settings.enableVulkanValidation ? ["VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation"] : []),
      ...(settings.saveRunLog ? ["FIVEINVADER_SAVE_RUN_LOG=1"] : []),
      enginePath,
      ...args,
    ];
    send("engine:status", { running: true, command });
    const childEnv = { ...process.env };
    if (settings.enableMangoHud) childEnv.MANGOHUD = "1";
    else delete childEnv.MANGOHUD;
    if (settings.enableVulkanValidation) childEnv.VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation";
    else delete childEnv.VK_INSTANCE_LAYERS;
    if (settings.saveRunLog) childEnv.FIVEINVADER_SAVE_RUN_LOG = "1";
    else delete childEnv.FIVEINVADER_SAVE_RUN_LOG;

    if (settings.saveRunLog) {
      const logDirectory = app.getPath("logs");
      await mkdir(logDirectory, { recursive: true });
      const stamp = new Date().toISOString().replaceAll(":", "-");
      const logPath = path.join(logDirectory, `fiveinvader-${stamp}.log`);
      runLog = createWriteStream(logPath, { flags: "a" });
      runLog.on("error", (error) => {
        send("engine:output", "stderr", `Unable to save run log: ${error.message}\n`);
        runLog = null;
      });
      send("engine:output", "stdout", `Saving engine output to ${logPath}\n`);
    }
    const child = spawn(enginePath, args, {
      cwd: path.dirname(enginePath),
      env: childEnv,
      stdio: ["pipe", "pipe", "pipe"],
      shell: false,
    });
    child.stdin.end();
    engineProcess = child;
    const forwardOutput = (stream: "stdout" | "stderr", chunk: Buffer) => {
      const text = chunk.toString();
      send("engine:output", stream, text);
      runLog?.write(`[${new Date().toISOString()}] [${stream}] ${text}`);
    };
    child.stdout.on("data", (chunk: Buffer) => forwardOutput("stdout", chunk));
    child.stderr.on("data", (chunk: Buffer) => forwardOutput("stderr", chunk));
    if (settings.timeoutSeconds > 0) {
      engineTimeout = setTimeout(() => {
        send("engine:output", "stderr", `Launcher timeout reached (${settings.timeoutSeconds}s); stopping engine.\n`);
        child.kill("SIGTERM");
      }, settings.timeoutSeconds * 1000);
    }
    child.on("error", (error) => {
      send("engine:output", "stderr", `Failed to start engine: ${error.message}\n`);
      if (engineTimeout) clearTimeout(engineTimeout);
      engineTimeout = null;
      runLog?.end();
      runLog = null;
      engineProcess = null;
      send("engine:status", { running: false, code: null });
    });
    child.on("close", (code, signal) => {
      if (engineTimeout) clearTimeout(engineTimeout);
      engineTimeout = null;
      runLog?.end();
      runLog = null;
      engineProcess = null;
      send("engine:status", { running: false, code, signal });
    });
    return { started: true, command };
  });
  ipcMain.handle("engine:stop", () => {
    if (!engineProcess) return false;
    engineProcess.kill("SIGTERM");
    return true;
  });

  createWindow();
  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

app.on("before-quit", () => {
  if (engineTimeout) clearTimeout(engineTimeout);
  engineProcess?.kill("SIGTERM");
});
