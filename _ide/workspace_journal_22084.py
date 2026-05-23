# 2026-05-13T15:58:50.328222400
import vitis

client = vitis.create_client()
client.set_workspace(path="Projects")

client.delete_component(name="app")

platform = client.create_platform_component(name = "CPU_INT",hw_design = "$COMPONENT_LOCATION/../../../Projects/MRICOBLAZE/CPU_Int_wrapper.xsa",os = "standalone",cpu = "microblaze_0",domain_name = "standalone_microblaze_0")

comp = client.create_app_component(name="app",platform = "$COMPONENT_LOCATION/../CPU_INT/export/CPU_INT/CPU_INT.xpfm",domain = "standalone_microblaze_0")

platform = client.get_component(name="CPU_INT")
status = platform.build()

comp = client.create_app_component(name="CPU_qq",platform = "$COMPONENT_LOCATION/../CPU_INT/export/CPU_INT/CPU_INT.xpfm",domain = "standalone_microblaze_0")

status = platform.build()

comp = client.get_component(name="CPU_qq")
comp.build()

vitis.dispose()

