const { app, BrowserWindow, ipcMain, Tray, Menu, nativeImage } = require('electron');
const path = require('path');

const auraEngine = require('./build/Release/aura_engine.node');

let mainWindow = null;
let tray = null;
let isQuitting = false;
let trayNoticeShown = false;

function updateTrayMenu() {
    if (!tray) return;
    const running = auraEngine.isRunning();
    const contextMenu = Menu.buildFromTemplate([
        {
            label: 'Open Aura Pulse',
            click: () => {
                if (mainWindow) {
                    mainWindow.show();
                    mainWindow.focus();
                }
            }
        },
        {
            label: running ? 'Stop Streaming' : 'Idle (Not Streaming)',
            enabled: running,
            click: () => {
                auraEngine.stop();
                updateTrayMenu();
                if (mainWindow && !mainWindow.isDestroyed()) {
                    mainWindow.webContents.send('tray:stopped');
                }
            }
        },
        { type: 'separator' },
        {
            label: 'Quit Aura Pulse',
            click: () => {
                isQuitting = true;
                auraEngine.stop();
                app.quit();
            }
        }
    ]);
    tray.setContextMenu(contextMenu);
    tray.setToolTip(running ? 'Aura Pulse — Streaming Active' : 'Aura Pulse — Ready');
}

function createTray() {
    const iconPath = path.join(__dirname, 'icon.png');
    const trayIcon = nativeImage.createFromPath(iconPath).resize({ width: 16, height: 16 });
    tray = new Tray(trayIcon);

    tray.on('click', () => {
        if (mainWindow) {
            if (mainWindow.isVisible()) {
                mainWindow.focus();
            } else {
                mainWindow.show();
            }
        }
    });

    updateTrayMenu();
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 840,
        height: 700,
        minWidth: 740,
        minHeight: 620,
        backgroundColor: '#0d1326',
        icon: path.join(__dirname, 'icon.png'),
        autoHideMenuBar: true,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    mainWindow.loadFile('index.html');

    // Minimize to System Tray instead of killing active audio streams on X
    mainWindow.on('close', (event) => {
        if (!isQuitting) {
            event.preventDefault();
            mainWindow.hide();
            if (!trayNoticeShown && tray) {
                trayNoticeShown = true;
                tray.displayBalloon({
                    iconType: 'info',
                    title: 'Aura Pulse',
                    content: 'Running in the system tray. Right-click the tray icon to stop or quit.'
                });
            }
        }
    });
}

// IPC Handlers
ipcMain.handle('devices:list', () => {
    return auraEngine.getOutputDevices();
});

ipcMain.handle('engine:start', (_event, targetIds) => {
    const res = auraEngine.start(targetIds);
    updateTrayMenu();
    return res;
});

ipcMain.handle('engine:stop', () => {
    auraEngine.stop();
    updateTrayMenu();
    return true;
});

ipcMain.handle('engine:status', () => {
    return auraEngine.isRunning();
});

ipcMain.handle('engine:setVolume', (_event, deviceId, volume) => {
    auraEngine.setVolume(deviceId, volume);
    return true;
});

ipcMain.handle('engine:setDelay', (_event, deviceId, delayMs) => {
    auraEngine.setDelay(deviceId, delayMs);
    return true;
});

app.whenReady().then(() => {
    createWindow();
    createTray();

    // Forward Windows WASAPI hotplug events to the UI
    auraEngine.onDevicesChanged(() => {
        updateTrayMenu();
        if (mainWindow && !mainWindow.isDestroyed()) {
            mainWindow.webContents.send('devices:changed');
        }
    });
});

app.on('before-quit', () => {
    isQuitting = true;
    auraEngine.stop();
});