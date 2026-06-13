# Item Finder Extension for Skript Proxy

## Overview
The Item Finder extension adds a `/find` command that opens a GUI window to search for items in the Growtopia items database.

## Features
- **Search by name**: Type any part of an item name to find matching items
- **Search by ID**: Enter an item ID directly to find that specific item
- **Real-time search**: Results update as you type
- **Detailed info**: Shows Item ID, Item Name, and Description (Type, Rarity, Clothing Type)
- **Fast and efficient**: Built with native Windows API for optimal performance

## Usage

### In-Game Command
Simply type in the game:
```
/find
```

Or search for a specific item:
```
/find dirt
/find laser
/find 242
```

### GUI Window
- A window titled "Item Finder - Skript Proxy" will open
- Type in the search box to search for items
- Results appear in real-time below
- You can see:
  - **Item ID**: The numeric ID of the item
  - **Item Name**: The display name
  - **Description**: Type, rarity, and clothing info

### Item Types
The extension recognizes these item types:
- Block, Door, Lock, Gems, Sign
- Platform, Bedrock, Lava
- Foreground, Background
- Seed, Clothing
- Consumable, Treasure
- And many more...

## Setup

### Prerequisites
The extension needs the `decoded_items.json` file from your items decoder.

### File Location
Place the `decoded_items.json` in one of these locations:
1. `C:\Users\11User\OneDrive\Desktop\botrdp\dtm\itemsdecoder\decoded_items.json` (default)
2. `./decoded_items.json` (proxy root directory)
3. `./resources/decoded_items.json`
4. `../decoded_items.json`

### Building
The extension is automatically compiled when you build the proxy. Make sure you have:
- Windows SDK (for GUI components)
- nlohmann/json library (should already be in your dependencies)

## Technical Details

### Files Created
- `src/extension/item_finder/item_finder.hpp` - Core database and search logic
- `src/extension/item_finder/item_finder_gui.hpp` - Windows GUI implementation
- `src/extension/item_finder/item_finder_extension.hpp` - Extension integration

### Architecture
- **ItemDatabase**: Loads and indexes items from JSON
- **ItemFinderGUI**: Creates native Windows ListView for search results
- **ItemFinderExtension**: Integrates with proxy command system

### Security
- GUI runs in a separate thread to avoid blocking the proxy
- Only one GUI window can be open at a time
- Command is cancelled client-side (not sent to server)

## Troubleshooting

### "Item database not loaded" Error
- Ensure `decoded_items.json` exists in one of the supported paths
- Check that the JSON file is valid and not corrupted
- Verify file permissions allow reading

### GUI doesn't appear
- Check Windows Firewall/antivirus isn't blocking the window
- Ensure you have Windows Desktop permissions
- Try running as administrator

### Search returns no results
- Check your search query (minimum 2 characters required)
- Verify the item exists in your items.dat version
- Try searching by ID instead of name

## Future Enhancements
Potential improvements:
- Favorite items list
- Export search results
- Advanced filters (by type, rarity, etc.)
- Item icons/preview
- Direct item spawning (if server allows)

## Credits
- Developed for Skript Proxy
- Uses items.dat decoder by the community
- GUI built with Windows API
