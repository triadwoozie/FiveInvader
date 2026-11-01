interface GameInspection {
  defaultXexPresent: boolean;
  defaultXexBytes: number;
  archiveCount: number;
  archives: { name: string; bytes: number }[];
  totalArchiveBytes: number;
}

interface LaunchSettings {
  gameDir: string;
  configPath?: string;
  language: string;
  resolution: string;
  vsync: boolean;
  fullscreen: boolean;
  fps: number;
  enableMangoHud: boolean;
}

interface ControllerSettings {
  bindings: Record<string, string>;
  leftDeadzone: number;
  rightDeadzone: number;
  triggerThreshold: number;
  vibration: boolean;
  invertLookY: boolean;
}

interface Window {
  fiveinvader: {
    defaults: () => Promise<{ gameDir: string; enginePath: string; running: boolean }>;
    chooseGameDir: () => Promise<string | null>;
    chooseConfig: () => Promise<string | null>;
    readConfig: (configPath: string) => Promise<string>;
    writeConfig: (configPath: string, contents: string) => Promise<boolean>;
    readControllerSettings: () => Promise<ControllerSettings>;
    writeControllerSettings: (settings: ControllerSettings) => Promise<boolean>;
    inspectGame: (gameDir: string) => Promise<GameInspection>;
    launch: (settings: LaunchSettings) => Promise<{ started: boolean; command: string[] }>;
    stop: () => Promise<boolean>;
    windowAction: (action: "minimize" | "close") => Promise<void>;
    onOutput: (callback: (stream: "stdout" | "stderr", text: string) => void) => () => void;
    onStatus: (callback: (status: { running: boolean; code?: number | null; signal?: string }) => void) => () => void;
  };
}
