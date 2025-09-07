# HC Web Finder - ULTRA-SAFE ASCII Map

A minimal ASCII map visualizer with device tracking demo. This project creates a retro-style ASCII map display with an animated ESP device tracker.

## Features

- **Zero-dependency**: Pure HTML, CSS, and JavaScript - no external libraries
- **ASCII Art Display**: Dynamic ASCII grid representing a map with roads and landmarks
- **Device Tracking**: Animated ESP device marker that orbits around the map center
- **Interactive Controls**:
  - Start/Stop animation
  - Regenerate road layout
  - Zoom in/out functionality
- **Responsive Design**: Scales appropriately on different screen sizes
- **Retro Aesthetic**: Monospace font with terminal-like styling

## Getting Started

1. Simply open `index.html` in any modern web browser
2. The demo starts automatically - you'll see an ESP device marker orbiting the map
3. Use the control buttons to interact with the visualization

## Controls

- **Start Demo**: Begin the device tracking animation
- **Stop**: Pause the animation
- **Regenerate Roads**: Create a new random road layout
- **+ Zoom / - Zoom**: Adjust the map scale

## Technical Details

- **Grid Size**: 96 columns × 32 rows
- **Coordinate System**: Based on Copenhagen, Denmark (55.6761°N, 12.5683°E)
- **Animation**: 150ms refresh rate with smooth orbital motion
- **Scale**: Configurable meters per cell (default: 8 meters)

## Browser Compatibility

Works in all modern browsers that support:
- ES6 JavaScript features
- CSS custom properties (variables)
- CSS clamp() function

No server required - this is a completely client-side application.
