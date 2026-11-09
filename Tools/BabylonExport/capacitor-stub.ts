// Stand-in for the phone-only Capacitor modules: the export runs in a desktop browser, never on a phone.
export const Capacitor = { isNativePlatform: () => false, getPlatform: () => 'web', isPluginAvailable: () => false };
export function registerPlugin(): any { return new Proxy({}, { get: () => async () => ({}) }); }
export const Filesystem: any = {}, Directory: any = {}, Encoding: any = {}, App: any = {}, Share: any = {}, Haptics: any = {}, ImpactStyle: any = {};
export default {};
