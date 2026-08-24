# F-Droid release path

The Android project can produce the unsigned release APK F-Droid expects entirely from public source and standard Android build inputs.

1. Keep `versionCode` and `versionName` in `app/build.gradle.kts` equal to the tagged public release.
2. Run the `F-Droid release build` workflow; it builds `assembleRelease`, verifies package identity and all three native ABIs, and retains the unsigned APK as evidence.
3. Keep `LICENSE`, `THIRD_PARTY.md`, and the F-Droid metadata aligned.
4. Tag the exact release commit `v<versionName>`.
5. Replace `FULL_COMMIT_HASH` in the metadata template with that tagged commit and submit it as `metadata/org.isomorphisms.ortho.yml` to fdroiddata.

F-Droid rebuilds and signs the application itself. The upstream unsigned APK is only a reproducibility gate.
