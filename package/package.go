package rapps

import (
	denv "github.com/jurgen-kluft/ccode/denv"
	respnow "github.com/jurgen-kluft/respnow/package"
	rsensors "github.com/jurgen-kluft/rsensors/package"
	ru8g2 "github.com/jurgen-kluft/ru8g2/package"
	rwifi "github.com/jurgen-kluft/rwifi/package"
)

const (
	repo_path = "github.com\\jurgen-kluft"
	repo_name = "rapps"
)

func GetPackage() *denv.Package {
	wifipkg := rwifi.GetPackage()
	sensorspkg := rsensors.GetPackage()
	u8g2pkg := ru8g2.GetPackage()
	espnowpkg := respnow.GetPackage()

	mainpkg := denv.NewPackage(repo_path, repo_name)
	mainpkg.AddPackage(wifipkg)
	mainpkg.AddPackage(sensorspkg)
	mainpkg.AddPackage(u8g2pkg)
	mainpkg.AddPackage(espnowpkg)

	// Setup the main applications
	airquality := denv.SetupCppAppProject(mainpkg, "airquality", "airquality")
	airquality.AddDependencies(wifipkg.GetMainLib())
	airquality.AddDependency(sensorspkg.GetLibrary("library_bh1750"))
	airquality.AddDependency(sensorspkg.GetLibrary("library_bme280"))
	airquality.AddDependency(sensorspkg.GetLibrary("library_scd41"))
	airquality.AddDependency(sensorspkg.GetLibrary("library_rd03d"))

	humanpresence := denv.SetupCppAppProject(mainpkg, "humanpresence", "humanpresence")
	humanpresence.AddDependencies(wifipkg.GetMainLib())
	humanpresence.AddDependencies(sensorspkg.GetMainLib())

	magnet := denv.SetupCppAppProjectForArduinoEsp8266(mainpkg, "magnet", "magnet")
	magnet.AddDependencies(wifipkg.GetMainLib())
	magnet.AddDependencies(sensorspkg.GetMainLib())
	magnet.AddDependencies(espnowpkg.GetMainLib())

	sh1107 := denv.SetupCppAppProjectForArduinoEsp32(mainpkg, "sh1107", "sh1107")
	sh1107.AddDependencies(wifipkg.GetMainLib())
	sh1107.AddDependencies(sensorspkg.GetMainLib())
	sh1107.AddDependencies(u8g2pkg.GetMainLib())

	mg58f18 := denv.SetupCppAppProjectForArduinoEsp32(mainpkg, "mg58f18", "mg58f18")
	mg58f18.AddDependencies(wifipkg.GetMainLib())
	mg58f18.AddDependencies(sensorspkg.GetMainLib())
	mg58f18.AddDependencies(u8g2pkg.GetMainLib())

	rd03d := denv.SetupCppAppProject(mainpkg, "rd03d", "rd03d")
	rd03d.AddDependencies(wifipkg.GetMainLib())
	rd03d.AddDependencies(sensorspkg.GetMainLib())

	hsp24 := denv.SetupCppAppProject(mainpkg, "hsp24", "hsp24")
	hsp24.AddDependencies(wifipkg.GetMainLib())
	hsp24.AddDependencies(sensorspkg.GetMainLib())

	mainpkg.AddMainApp(airquality)
	mainpkg.AddMainApp(humanpresence)
	mainpkg.AddMainApp(magnet)
	mainpkg.AddMainApp(sh1107)
	mainpkg.AddMainApp(mg58f18)
	mainpkg.AddMainApp(rd03d)
	mainpkg.AddMainApp(hsp24)

	return mainpkg
}
