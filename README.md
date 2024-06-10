# t5zm_bot - external BO1 zombies cheat

![zombies](https://i.imgur.com/MlwNfeG.png)

## Overview

This repository contains a C++ project that demonstrates an external aimbot and ESP for Call of Duty: Black Ops Zombies. This project is intended as a learning project and is designed to work exclusively in solo mode.

> **Disclaimer:** This project is for educational purposes only. Cheating in online games is unethical and can result in bans or other penalties. Use this code responsibly and at your own risk.

## Features

- **Smooth Aimbot** 
- **ESP** 
  - **Name** 
  - **Distance** 
  - **Healthbar**
- **Crosshair**

## Key Bindings

- **Aimbot Hold Key:** `mouse4`
- **Menu Toggle:** `INS`
- **Menu Up/Down:** `Up/Down Arrow Keys`
- **Menu Select:** `Right Arrow Key`

## Known issues / needed improvements

- ESP box can get really narrow or invisible at some angles.
- Aim is based on zombie's origin position + a hardcoded aim height, rather than bone position. Can be inaccurate sometimes but is good enough.
- WorldToScreen is hardcoded to 2560x1080 resolution. 
