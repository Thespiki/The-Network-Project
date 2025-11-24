# 🔌 The Network Project (TNP)

The Network Project (TNP) is an application designed to let anyone create, configure, and simulate a computer network without requiring advanced technical knowledge. Its purpose is to make network design simple, intuitive, and fully guided (for beginners) or automate tasks, configurations or gain time in networking setups (for advanced).

## 🎯 Goal
Provide an accessible and complete tool to imagine, prototype, and configure a network whether it's a small home setup or a large-scale environment such as a school, business, or LAN event.

## 🚀 Main Features

### 🗺️ Network Creation & Management
- Build network topologies visually: routers, switches, modems, servers, computers, and more.
- Connect and organize elements into a functional infrastructure.
- Manage multiple projects and different types of networks.

### 🧩 Hardware & Service Configuration
- Configure servers, routers, and machines with just a few clicks.
- Generate recommended hardware configurations based on your needs or budget.
- Quickly deploy services such as:  
  - Web servers  
  - Databases  
  - Game servers  
  - And more, hopefully with an active community

### 🧪 Network Simulation
- Simulate how your network will behave before deploying it.
- View performance, limitations, and resource requirements.
- Identify weaknesses and potential improvements.

### 📚 Tutorial Mode
- Learn step-by-step how to build and understand a network.
- Discover the basics of routing, services, security, and more.
- This part is ideal for beginners, students, and curious users.

### 🛠️ Open Source
TNP is fully open source.  
The community will be able to contribute to:
- The hardware catalog  
- Server templates  
- Simulation improvements  
- Tutorials, documentation, optimisations and bug fixes

## 🔧 Project Status
**🛠 The project is currently under development. A beta version will be released soon to allow early contributions.**

Stay tuned for updates ^^

---

## 🧰 Getting Started (Desktop UI Prototype)

This repository now includes a minimal desktop application scaffold built with C++ and Qt 6 (Widgets) using CMake. The UI follows a strictly squared, black & white theme.

### Prerequisites
- CMake 3.21+
- A C++17 compiler (MSVC, Clang, or GCC)
- Qt 6.2+ (Widgets)

On Windows, install Qt via the Qt Online Installer, then note your Qt 6 installation path (e.g., `C:\Qt\6.6.2\msvc2022_64`).

### Configure and Build
From the project root:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc20xx_64"
cmake --build build --config Release
```

If you use Ninja or a different generator, add `-G "Ninja"`. IDEs like CLion can detect and configure CMake automatically; set `CMAKE_PREFIX_PATH` to your Qt 6 directory in the CMake profile.

### Run
The app target is `tnp`. After building:

```
./build/tnp            # On Linux/macOS
build/Release/tnp.exe  # On Windows (with multi-config generators)
```

### Project Layout
- `CMakeLists.txt` — CMake build script
- `src/` — application sources
  - `main.cpp` — app entry
  - `MainWindow.cpp` — main window and layout
  - `CanvasView.cpp` — central canvas with a squared grid
- `include/` — headers
- `resources/` — Qt resource collection + global stylesheet (`styles.qss`)

## 🎨 Design Constraints
- Strictly black and white UI
  - Backgrounds: white `#fff`
  - Text and borders: black `#000`
  - Overlays only use transparent black (e.g., `rgba(0,0,0,0.08)`) for hover/selection
- Squared style everywhere
  - No rounded corners (`border-radius: 0`)
  - Straight lines and rectangular shapes

These constraints are enforced globally via a Qt Stylesheet (`resources/styles.qss`).

## ✨ Current UI Shell
- Top toolbar with `New`, `Open`, `Save`
- Left palette dock with basic items: Router, Switch, Server, PC, Modem
- Central canvas with a subtle square grid (black lines on white)
- Right properties dock (placeholder)
- Bottom status bar

This provides a clean foundation to implement the topology editor, node tools, and property inspectors while keeping the design system consistent.
