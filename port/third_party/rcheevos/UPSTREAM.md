# rcheevos

Source: https://github.com/RetroAchievements/rcheevos

Stable master revision: `1433173220a7eaede6a9ed7a18e94117be1821e0` (12.5.0).
Vendored `include/` and `src/` without changes. Only the runtime, client, API and MD5
sources are compiled, with `RC_DISABLE_LUA` and without optional external clients,
RAIntegration or the general-purpose disc hash subsystem.

License: MIT (`LICENSE`). The MD5 implementation retains its own permissive notice
at the top of `src/rhash/md5.h` and `md5.c`.
