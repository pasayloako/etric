#!/usr/bin/env python3
"""Fetch and bundle the real Cebu OSM roads/buildings used by the game.

This writes a small, metadata-stripped OSM XML file. The game reads that bundled
asset at runtime, so a completed build does not require an internet connection.
"""

from __future__ import annotations

import argparse
import json
import os
import tempfile
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path


SOUTH, WEST, NORTH, EAST = 10.2745, 123.9555, 10.2840, 123.9650
OVERPASS = "https://overpass-api.de/api/interpreter"
TAGS_TO_KEEP = {
    "highway",
    "building",
    "natural",
    "name",
    "ref",
    "oneway",
    "junction",
    "height",
    "building:levels",
    "power",
    "amenity",
    "landuse",
    "surface",
}


def fetch() -> dict:
    box = f"{SOUTH},{WEST},{NORTH},{EAST}"
    query = (
        "[out:json][timeout:45];("
        f'way["highway"]({box});'
        f'way["building"]({box});'
        f'way["natural"="water"]({box});'
        f'node["natural"="tree"]({box});'
        f'node["power"="pole"]({box});'
        f'node["highway"="street_lamp"]({box});'
        f'node["highway"="traffic_signals"]({box});'
        ");out body geom;"
    )
    body = urllib.parse.urlencode({"data": query}).encode()
    request = urllib.request.Request(
        OVERPASS,
        data=body,
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            "User-Agent": "IMISUBEBE-ETricycle/1.0 (offline OSM data; attribution in app)",
        },
    )
    with urllib.request.urlopen(request, timeout=90) as response:
        payload = json.load(response)
    elements = payload.get("elements", [])
    if not isinstance(elements, list) or not elements:
        raise RuntimeError("Overpass returned no map elements.")
    return payload


def convert(payload: dict) -> ET.Element:
    root = ET.Element("osm", {"version": "0.6"})
    nodes: dict[int, dict] = {}
    ways = []

    for element in payload["elements"]:
        if element.get("type") == "node":
            nodes[int(element["id"])] = element
        elif element.get("type") == "way":
            ways.append(element)
            refs = element.get("nodes", [])
            geometry = element.get("geometry", [])
            for ref, point in zip(refs, geometry):
                node_id = int(ref)
                nodes.setdefault(
                    node_id,
                    {"id": node_id, "lat": point["lat"], "lon": point["lon"], "tags": {}},
                )

    for node_id, node in sorted(nodes.items()):
        attrs = {
            "id": str(node_id),
            "lat": f'{float(node["lat"]):.7f}',
            "lon": f'{float(node["lon"]):.7f}',
        }
        out = ET.SubElement(root, "node", attrs)
        for key, value in sorted(node.get("tags", {}).items()):
            if key in TAGS_TO_KEEP:
                ET.SubElement(out, "tag", {"k": str(key), "v": str(value)})

    for way in sorted(ways, key=lambda item: int(item["id"])):
        out = ET.SubElement(root, "way", {"id": str(way["id"])})
        for ref in way.get("nodes", []):
            ET.SubElement(out, "nd", {"ref": str(ref)})
        for key, value in sorted(way.get("tags", {}).items()):
            if key in TAGS_TO_KEEP:
                ET.SubElement(out, "tag", {"k": str(key), "v": str(value)})

    ET.indent(root, space=" ")
    return root


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1]
        / "app/src/main/assets/osm_cebu.osm",
    )
    args = parser.parse_args()
    payload = fetch()
    root = convert(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    fd, temporary_name = tempfile.mkstemp(
        prefix=args.output.name + ".", dir=args.output.parent
    )
    try:
        with os.fdopen(fd, "wb") as destination:
            ET.ElementTree(root).write(
                destination, encoding="utf-8", xml_declaration=True
            )
        os.replace(temporary_name, args.output)
    except Exception:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise

    print(
        f"Saved {len(root.findall('way'))} ways and "
        f"{len(root.findall('node'))} nodes to {args.output}"
    )


if __name__ == "__main__":
    main()
