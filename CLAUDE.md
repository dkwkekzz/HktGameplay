# CLAUDE.md

## Project Overview

3d rts adventure dynamic mmorpg's module 
[adventure: item attribute and combination, random growth]
[dynamic: realtime build adventure configurance by mcp server]

## Architecture

## Important Constraints

1. **No UE5 runtime in HktCore** — keep the VM pure C++ (no UObject, UWorld, etc.)
2. **Server-authoritative** — clients cannot manipulate what they see; server filters all data
