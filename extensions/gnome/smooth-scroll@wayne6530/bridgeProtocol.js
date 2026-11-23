export const BUS_NAME = 'org.gnome.Shell';
export const BUS_PATH = '/org/gnome/Shell/Extensions/SmoothScroll';
export const BUS_IFACE = 'org.gnome.Shell.Extensions.SmoothScroll';
export const BUS_XML = `<node>
  <interface name="${BUS_IFACE}">
    <method name="PickWindow"><arg name="window" type="s" direction="out"/></method>
    <method name="CancelPick"/>
    <method name="GetStatus"><arg name="status" type="s" direction="out"/></method>
  </interface>
</node>`;
