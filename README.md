<div align="center">
<img src="https://github.com/qbic/cyb-engine-resources/blob/main/cyb-engine-2024.03.png" width="512">
  
# `cyb-engine`
  
![GitHub top language](https://img.shields.io/github/languages/top/qbic/cyb-engine)
![GitHub last commit](https://img.shields.io/github/last-commit/qbic/cyb-engine)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)

<p class="align center">
<h4><code>cyb-engine</code> is an open-source, easy to use and easy to modify 3d-engine</h4>
</p>
</div>

## Getting started...
Run the following commands to clone the git repository and generate project files
for visual studio 2022
```bash
git clone https://github.com/qbic/cyb-engine
cd cyb-engine && tools\generate-project-files.bat
```

## Dependency graph
```mermaid
flowchart TD
    Game --> HLI
    Game --> Input
    HLI --> Renderer
    HLI --> Editor
    Renderer --> Systems
    Systems --> Graphics
    Graphics --> Core
    Input --> Core
    Editor --> Renderer
```