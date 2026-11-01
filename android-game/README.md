# IMISUBEBE E-Tricycle Simulator

A standalone Android game built as a native C++ `NativeActivity`. Gameplay and
rendering run in C++ with the Android NDK, CMake, and OpenGL ES 3.0. There is
no WebView, embedded web page, or JavaScript game layer.

## Open the project

Open this `android-game` folder in Android Studio and allow Gradle sync to
finish. The project uses:

- Android Gradle Plugin 8.8.2 and Gradle 8.10.2
- Android SDK Platform 35
- Android NDK 27.2.12479018
- CMake 3.22.1
- Android 8.0 (API 26) or newer, with OpenGL ES 3.0

Android Studio can install the pinned SDK, NDK, and CMake versions from its
SDK Manager. The untracked `local.properties` file is generated for each
machine; do not commit it.

## Build APKs

From this directory:

```sh
./gradlew assembleDebug
```

The signed, installable debug APK is written to:

```text
app/build/outputs/apk/debug/app-debug.apk
```

Build the release variant with:

```sh
./gradlew assembleRelease
```

The release variant is signed with the standard Android debug key so it can be
installed for local testing. For a store or public release, create and protect
your own signing key, then configure Android Studio's **Build > Generate Signed
Bundle / APK** flow. Never commit a private signing key.

## Play

Landscape is recommended; portrait is supported. Touch pedals and steering
buttons can be held together. The reverse button toggles between forward and
reverse gear, and GAS then drives in the selected direction.

1. Tap **START MISSION**.
2. Follow the blue route on the real-road minimap to the waiting passenger.
3. Stop near the passenger and tap **PICK UP**.
4. Follow the mapped route to the red destination marker.
5. Stop inside the drop-off area and tap **DROP OFF** to receive 250 coins.

**CAMERA** cycles rear, front, driver, and free views. Drag the screen in free
view to orbit the camera. **SET** opens day/night and rain controls. **HORN**
and **II** are also active game controls. Keyboard testing is supported with
WASD / arrow keys, R for reverse, C for camera, M for settings, Space for horn,
and Enter to start.

## Offline OpenStreetMap data

The game packages `app/src/main/assets/osm_cebu.osm`; it does not download a
map when it starts. That file contains a snapshot of real OpenStreetMap road
geometry and building footprints for:

- South: `10.2745`, west: `123.9555`
- North: `10.2840`, east: `123.9650`
- Pickup: `10.2802038, 123.9606781`
- Destination: `10.2782869, 123.9588547`

The current snapshot includes 315 drivable road ways and 236 building
footprints. The game builds 3D road surfaces, junctions, lane markings,
sidewalks, buildings, navigation, and the minimap from this local data. A
missing or disconnected road graph is reported as an error; the game does not
substitute invented roads.

If the snapshot lacks tagged traffic-light nodes, simple traffic lights and
Philippine-style PARE signs are placed at mapped road intersections. These are
environment props, not live traffic-control data.

To refresh the bundled data while online, run:

```sh
python3 tools/fetch_osm.py
```

The refresh script writes a reduced OSM XML snapshot atomically and preserves
the existing asset if the download fails. The in-game settings panel and
`OSM_ATTRIBUTION.txt` include OpenStreetMap attribution and the ODbL license.

## Controls and systems

- Native touch UI adapts to portrait and landscape: GAS, BRAKE, LEFT, RIGHT,
  REVERSE, PICK UP / DROP OFF, CAMERA, HORN, SETTINGS, and PAUSE.
- C++ vehicle model with acceleration, braking, reverse, steering, wheel
  rotation, spring response, and mapped-road / traffic collision checks.
- Real OSM shortest-path routing respects mapped one-way streets.
- Passenger mission and persistent coin rewards.
- Low-poly Philippine-style tricycle, jeepney-like traffic, cars,
  motorcycles, passengers, and pedestrians. Nearby traffic uses more geometry;
  distant actors are simplified or culled.
- A timed day/night cycle, rain particles, synthesized electric-motor sound,
  and horn sound are native runtime features.

## Verification and current visual scope

`app/src/main/cpp/tests/core_tests.cpp` runs on the host with CMake/CTest. It
checks the bundled OSM graph, route connectivity, map coverage, acceleration,
braking, steering, and settings/weather state. APK build commands above use
the same C++ sources through the Android NDK.

The 3D scene uses generated low-poly geometry and solid colors instead of
external image textures. Trees and streetlights are placed along actual OSM
roads when the source snapshot has no mapped instances; traffic and walking
actors are simulated on the real road geometry, not live traffic feeds.
Weather is a visual rain effect (not wet-road physics), and traffic AI is
lightweight. An Android emulator/device visual and input play-test is still
recommended after installing the APK.
