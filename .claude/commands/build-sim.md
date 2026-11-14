Build the native desktop simulator:
```bash
cmake -B build-native -S . && cmake --build build-native -j4
```
Run it:
```bash
./build-native/phase_box_native_sim
```
