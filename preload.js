const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('auraAPI', {
  getOutputDevices: () => ipcRenderer.invoke('devices:list'),
  start: (targetIds) => ipcRenderer.invoke('engine:start', targetIds),
  stop: () => ipcRenderer.invoke('engine:stop'),
  isRunning: () => ipcRenderer.invoke('engine:status'),
  setVolume: (deviceId, volume) => ipcRenderer.invoke('engine:setVolume', deviceId, volume),
  setDelay: (deviceId, delayMs) => ipcRenderer.invoke('engine:setDelay', deviceId, delayMs),
  onDevicesChanged: (callback) => {
    ipcRenderer.removeAllListeners('devices:changed');
    ipcRenderer.on('devices:changed', () => callback());
  },
  onTrayStop: (callback) => {
    ipcRenderer.removeAllListeners('tray:stopped');
    ipcRenderer.on('tray:stopped', () => callback());
  }
});