# Continuous integration

`ci.yml` runs on every push and pull request.

**`build`** compiles and tests on Windows (MSVC) and Linux (GCC) with warnings treated as
errors, then drives `tnpcli` through a full workflow — generate a project, validate it, run
its tests, ping across it, export it, convert it between formats and validate the result.
That last step is the closest CI can get to using the application, because the headless tool
drives the same `Application` object the window does.

**`headless`** builds with `TNP_BUILD_UI=OFF`. It exists to enforce a layering rule that is
easy to break by accident: nothing below `src/ui` may depend on GLFW or Dear ImGui. If someone
includes an ImGui header in the simulator, this job fails and the graphical builds do not.

macOS is not built in CI. The code is portable and is developed on macOS, but the project
targets Windows first and a third runner is not yet earning its cost.
