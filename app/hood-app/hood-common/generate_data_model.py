#!/usr/bin/env python3

"""Build the XT-ZB2 hood Matter data model from upstream Matter examples."""

from __future__ import annotations

import copy
import json
from pathlib import Path


CHIP_ROOT = Path(__file__).resolve().parents[3]
OUTPUT = Path(__file__).with_name("hood-app.zap")
LIGHTING = CHIP_ROOT / "examples/lighting-app/lighting-common/lighting-app.zap"
FAN = CHIP_ROOT / "examples/chef/devices/rootnode_fan_7N2TobIlOX.zap"
ON_OFF_LIGHT = CHIP_ROOT / "examples/lighting-app/nxp/zap/lighting-on-off.zap"


def load(path: Path) -> dict:
    with path.open(encoding="utf-8") as source:
        return json.load(source)


def set_attribute(cluster: dict, name: str, default: str, storage: str = "RAM") -> None:
    for attribute in cluster["attributes"]:
        if attribute["name"] == name:
            attribute["defaultValue"] = default
            attribute["storageOption"] = storage
            return
    raise RuntimeError(f"Fan Control attribute not found: {name}")


def main() -> None:
    lighting = load(LIGHTING)
    fan = load(FAN)
    on_off_light = load(ON_OFF_LIGHT)

    root_type = copy.deepcopy(lighting["endpointTypes"][0])
    fan_type = copy.deepcopy(fan["endpointTypes"][1])
    light_type = copy.deepcopy(on_off_light["endpointTypes"][1])

    root_type["id"] = 1
    root_type["name"] = "HOOD-root"
    root_type["deviceTypes"] = [root_type["deviceTypes"][0]]
    root_type["deviceVersions"] = [root_type["deviceVersions"][0]]
    root_type["deviceIdentifiers"] = [root_type["deviceIdentifiers"][0]]
    root_type["clusters"] = [
        cluster
        for cluster in root_type["clusters"]
        if cluster["name"]
        in {
            "Descriptor",
            "Access Control",
            "Basic Information",
            "General Commissioning",
            "Network Commissioning",
            "General Diagnostics",
            "Thread Network Diagnostics",
            "Administrator Commissioning",
            "Operational Credentials",
            "Group Key Management",
        }
    ]

    fan_type["id"] = 2
    fan_type["name"] = "HOOD-fan"
    fan_type["clusters"] = [
        cluster
        for cluster in fan_type["clusters"]
        if cluster["name"] in {"Identify", "Groups", "Descriptor", "Fan Control"}
    ]
    fan_cluster = next(cluster for cluster in fan_type["clusters"] if cluster["name"] == "Fan Control")
    wanted_fan_attributes = {
        "FanMode",
        "FanModeSequence",
        "PercentSetting",
        "PercentCurrent",
        "SpeedMax",
        "SpeedSetting",
        "SpeedCurrent",
        "GeneratedCommandList",
        "AcceptedCommandList",
        "AttributeList",
        "FeatureMap",
        "ClusterRevision",
    }
    fan_cluster["attributes"] = [
        attribute for attribute in fan_cluster["attributes"] if attribute["name"] in wanted_fan_attributes
    ]
    fan_cluster["commands"] = []
    set_attribute(fan_cluster, "FanMode", "0", "NVM")
    set_attribute(fan_cluster, "FanModeSequence", "0")
    set_attribute(fan_cluster, "PercentSetting", "0", "NVM")
    set_attribute(fan_cluster, "PercentCurrent", "0")
    set_attribute(fan_cluster, "SpeedMax", "4")
    set_attribute(fan_cluster, "SpeedSetting", "0", "NVM")
    set_attribute(fan_cluster, "SpeedCurrent", "0")
    set_attribute(fan_cluster, "FeatureMap", "1")
    set_attribute(fan_cluster, "ClusterRevision", "5")

    light_type["id"] = 3
    light_type["name"] = "HOOD-light"
    light_type["clusters"] = [
        cluster
        for cluster in light_type["clusters"]
        if cluster["name"] in {"Identify", "Groups", "Descriptor", "On/Off"}
    ]
    light_cluster = next(cluster for cluster in light_type["clusters"] if cluster["name"] == "On/Off")
    set_attribute(light_cluster, "OnOff", "0", "NVM")

    lighting["endpointTypes"] = [root_type, fan_type, light_type]
    lighting["endpoints"] = [
        {
            "endpointTypeName": "HOOD-root",
            "endpointTypeIndex": 0,
            "profileId": 259,
            "endpointId": 0,
            "networkId": 0,
            "parentEndpointIdentifier": None,
        },
        {
            "endpointTypeName": "HOOD-fan",
            "endpointTypeIndex": 1,
            "profileId": 259,
            "endpointId": 1,
            "networkId": 0,
            "parentEndpointIdentifier": None,
        },
        {
            "endpointTypeName": "HOOD-light",
            "endpointTypeIndex": 2,
            "profileId": 259,
            "endpointId": 2,
            "networkId": 0,
            "parentEndpointIdentifier": 1,
        },
    ]

    with OUTPUT.open("w", encoding="utf-8") as destination:
        json.dump(lighting, destination, indent=2)
        destination.write("\n")


if __name__ == "__main__":
    main()
