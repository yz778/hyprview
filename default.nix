{
  lib,
  hyprland,
  hyprlandPlugins,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprview";
  version = "0.1";
  src = ./.;

  inherit (hyprland) nativeBuildInputs;

  meta = with lib; {
    homepage = "https://github.com/yz778/hyprview";
    description = "Hyprland Overview Plugin";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
