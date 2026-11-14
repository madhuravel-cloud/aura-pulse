const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const auraEngine = require('./build/Release/aura_engine.node');

function createWindow() {
    const win = new BrowserWindow({
        width: 780,
        height: 720,
        minWidth: 680,
        minHeight: 640,
        backgroundColor: '#0d1326',
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false
        }
    });

    win.setMenuBarVisibility(false);
    win.loadFile('index.html');
}

ipcMain.handle('devices:list', () => {
    return auraEngine.getOutputDevices();
});

ipcMain.handle('engine:start', (_event, targetIds) => {
    return auraEngine.start(targetIds);
});

ipcMain.handle('engine:stop', () => {
    auraEngine.stop();
    return true;
});

ipcMain.handle('engine:status', () => {
    return auraEngine.isRunning();
});

app.whenReady().then(createWindow);

app.on('will-quit', () => {
    auraEngine.stop();
});

app.on('window-all-closed', () => {
    app.quit();
});