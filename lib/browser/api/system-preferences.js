const { app } = require('electron')
const { EventEmitter } = require('events')
const { systemPreferences, SystemPreferences } = process.atomBinding('system_preferences')

// SystemPreferences is an EventEmitter.
Object.setPrototypeOf(SystemPreferences.prototype, EventEmitter.prototype)
EventEmitter.call(systemPreferences)

if (process.platform === 'darwin') {
  let appearanceTrackingSubscriptionID = null

  systemPreferences.startAppLevelAppearanceTrackingOS = () => {
    if (appearanceTrackingSubscriptionID !== null) return

    const updateAppearanceBasedOnOS = () => {
      const newAppearance = systemPreferences.isDarkMode()
        ? 'dark'
        : 'light'

      systemPreferences.emit('appearance-changed', newAppearance)
      systemPreferences.setAppLevelAppearance(newAppearance)
    }

    appearanceTrackingSubscriptionID = systemPreferences.subscribeNotification(
      'AppleInterfaceThemeChangedNotification',
      updateAppearanceBasedOnOS
    )

    updateAppearanceBasedOnOS()
  }

  systemPreferences.stopAppLevelAppearanceTrackingOS = () => {
    if (appearanceTrackingSubscriptionID === null) return

    systemPreferences.unsubscribeNotification(appearanceTrackingSubscriptionID)
  }

  app.on('quit', systemPreferences.stopAppLevelAppearanceTrackingOS)
}

module.exports = systemPreferences
