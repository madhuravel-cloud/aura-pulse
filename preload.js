const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('auraAPI', {
  getOutputDevices: () => ipcRenderer.invoke('devices:list'),
  start: (targetIds) => ipcRenderer.invoke('engine:start', targetIds),
  stop: () => ipcRenderer.invoke('engine:stop'),
  isRunning: () => ipcRenderer.invoke('engine:status')
});