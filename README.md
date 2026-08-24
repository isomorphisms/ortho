# Ortho

A small mathematical toy for handling orthants directly: render them, turn them in your hand, and see how the experience changes with dimension and sign choice.

The first slice is deliberately not a proof interface. It is a draggable three-dimensional positive orthant: three translucent coordinate-face patches meeting at a visible corner. One-finger drag rotates the object.

The mathematical geometry lives in Idriç under `src/`. `src/Generate.idric` emits `app/src/main/cpp/orthant_model.h`; the Android shell is the same small native pattern used by the other math viewers: `NativeActivity` + `android_native_app_glue` + EGL + GLES3.

## Android build

Requirements: JDK 17, Android SDK 36, NDK `29.0.14206865`, CMake 3.22.1, and Gradle 8.13.

```sh
gradle :app:assembleDebug
```

The APK is written under `app/build/outputs/apk/debug/`.

## Idriç model

`src/Orthant.idric` is the source of the face and edge vertices. The generated C header is checked in so an Android build does not need to bootstrap the compiler.

If an Idriç checkout is available at `../Idric`:

```sh
scripts/regenerate-model.sh
```

Set `IDRIC=/path/to/Idric/build/exec/idris2` to use a different checkout.

## Touch

- one finger: rotate the orthant
- lift: leave it where it is

Pinch, dimension changes, and sign changes are intentionally not in this first slice.

## Reference

Kenneth O. May, “The Impossibility of a Division Algebra of Vectors in Three Dimensional Space,” *American Mathematical Monthly* 73(3), 1966, 289–291. See `references/may-1966.md`.
