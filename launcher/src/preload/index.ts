import { contextBridge, ipcRenderer } from "electron";

const api = {
  defaults: () => ipcRenderer.invoke("launcher:defaults"),
  chooseGameDir: () => ipcRenderer.invoke("launcher:choose-game-dir"),
  chooseStreamDir: () => ipcRenderer.invoke("launcher:choose-stream-dir"),
  chooseConfig: () => ipcRenderer.invoke("launcher:choose-config"),
  readConfig: (configPath: string) => ipcRenderer.invoke("launcher:read-config", configPath),
  writeConfig: (configPath: string, contents: string) =>
    ipcRenderer.invoke("launcher:write-config", configPath, contents),
  readControllerSettings: () => ipcRenderer.invoke("launcher:read-controller-settings"),
  writeControllerSettings: (settings: ControllerSettings) =>
    ipcRenderer.invoke("launcher:write-controller-settings", settings),
  inspectGame: (gameDir: string) => ipcRenderer.invoke("launcher:inspect-game", gameDir),
  launch: (settings: {
    gameDir: string;
    configPath?: string;
    language: string;
    resolution: string;
    streamDir?: string;
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
  }) => ipcRenderer.invoke("engine:launch", settings),
  stop: () => ipcRenderer.invoke("engine:stop"),
  windowAction: (action: "minimize" | "close") => ipcRenderer.invoke("launcher:window-action", action),
  onOutput: (callback: (stream: "stdout" | "stderr", text: string) => void) => {
    const listener = (_event: Electron.IpcRendererEvent, stream: "stdout" | "stderr", text: string) =>
      callback(stream, text);
    ipcRenderer.on("engine:output", listener);
    return () => ipcRenderer.removeListener("engine:output", listener);
  },
  onStatus: (callback: (status: { running: boolean; code?: number | null; signal?: string }) => void) => {
    const listener = (_event: Electron.IpcRendererEvent, status: { running: boolean; code?: number | null; signal?: string }) =>
      callback(status);
    ipcRenderer.on("engine:status", listener);
    return () => ipcRenderer.removeListener("engine:status", listener);
  },
};

contextBridge.exposeInMainWorld("fiveinvader", api);
