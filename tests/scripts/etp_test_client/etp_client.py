#!/usr/bin/env python3
"""
ETP 1.2 Test Client for OSDU Reservoir DDMS (open-etp-server).

A command-line tool to send ETP 1.2 protocol messages to the RDDMS server.
Supports all 6 server protocols: Dataspace, Store, DataArray, Transaction,
Discovery, and SupportedTypes. Works in single-command or interactive mode.

SETUP:
    pip install websockets etpproto

SINGLE-COMMAND MODE (one command per invocation, session auto-closes):
    python etp_client.py --dns <host> --token <TOKEN> --partition dp1 <command> [options]

    Examples:
      python etp_client.py --dns instance.energy.azure.com --token $tok --partition dp1 ping
      python etp_client.py --dns instance.energy.azure.com --token $tok --partition dp1 get-dataspaces
      python etp_client.py --dns instance.energy.azure.com --token $tok --partition dp1 put-dataspace --path "my/space" --legaltag "dp1-tag"

INTERACTIVE MODE (session stays open, type commands at etp> prompt, Ctrl+C to exit):
    python etp_client.py --dns <host> --token <TOKEN> --partition dp1 interactive

    Example session:
      etp> get-dataspaces
      etp> put-dataspace --path "test/space" --legaltag "dp1-tag"
      etp> put-data-object --dataspace test/space --name my_grid
      etp> put-uninit-array --dataspace test/space --obj-uuid <uuid> --dims 4,4 --path /data/grid --type double
      etp> put-subarrays --dataspace test/space --obj-uuid <uuid> --path /data/grid --starts 0,0 --counts 2,4 --data 1,2,3,4,5,6,7,8
      etp> get-arrays --dataspace test/space --obj-uuid <uuid> --path /data/grid
      etp> start-transaction --dataspace test/space
      etp> commit-transaction --txn-uuid <uuid>
      etp> help
      etp> exit

COMMANDS:
    Core:           ping
    Dataspace(24):  get-dataspaces, put-dataspace, delete-dataspace
    Store(4):       put-data-object, get-data-objects, delete-data-objects
    DataArray(9):   put-uninit-array, put-arrays, put-subarrays, get-arrays, get-subarrays, get-metadata
    Transaction(18):start-transaction, commit-transaction, rollback-transaction
    Discovery(3):   get-resources
    SupportedTypes: get-supported-types

COMMAND REFERENCE:
    ping                                                    Send Ping, get Pong
    get-dataspaces                                          List all dataspaces
    put-dataspace     --path <path> --legaltag <tag>        Create a dataspace
                      [--viewers <acl>] [--owners <acl>]
                      [--countries <codes>]
    delete-dataspace  --path <path>                         Delete a dataspace
    put-data-object   --dataspace <ds> [--obj-uuid <uuid>]  Create EpcExternalPartReference
                      [--name <name>]
    get-data-objects  --dataspace <ds> --obj-uuid <uuid>    Get a data object
    delete-data-objects --dataspace <ds> --obj-uuid <uuid>  Delete a data object
    put-uninit-array  --dataspace <ds> --obj-uuid <uuid>    Create empty array
                      --dims <d1,d2,...> --path <path>
                      [--type double|float|int|long]
    put-arrays        --dataspace <ds> --obj-uuid <uuid>    Create + fill array
                      --dims <d1,d2,...> --path <path>
                      --data <v1,v2,...>
    put-subarrays     --dataspace <ds> --obj-uuid <uuid>    Write sub-region
                      --path <path> --starts <s1,s2,...>
                      --counts <c1,c2,...> --data <v1,v2,...>
    get-arrays        --dataspace <ds> --obj-uuid <uuid>    Read full array
                      --path <path>
    get-subarrays     --dataspace <ds> --obj-uuid <uuid>    Read sub-region
                      --path <path> --starts <s1,s2,...>
                      --counts <c1,c2,...>
    get-metadata      --dataspace <ds> --obj-uuid <uuid>    Get array metadata
                      --path <path>
    start-transaction [--dataspace <ds>]                    Start transaction
    commit-transaction --txn-uuid <uuid>                    Commit transaction
    rollback-transaction --txn-uuid <uuid>                  Rollback transaction
    get-resources     --dataspace <ds>                      Discover objects
    get-supported-types                                     List supported types

TYPICAL WORKFLOW:
    1. put-dataspace          -- create a dataspace with legal tag + ACLs
    2. put-data-object        -- create EpcExternalPartReference (container for arrays)
    3. put-uninit-array       -- allocate an empty array (dims + type)
    4. put-subarrays          -- write data into a sub-region
    5. get-arrays             -- read the full array back
    6. get-subarrays          -- read a sub-region
    7. delete-dataspace       -- clean up

    For transactional writes (interactive mode only):
    1. start-transaction      -- begin (scoped to a dataspace)
    2. put-data-object + put-uninit-array + put-subarrays  -- all within the txn
    3. commit-transaction     -- atomically persist, or rollback-transaction to discard

NOTE: In interactive mode, the ETP session stays open across commands (same WebSocket).
      In single-command mode, each invocation opens and closes its own session.
      The server may disconnect idle sessions after ~5 minutes.
"""
import argparse
import asyncio
import ssl
import uuid
from datetime import datetime, timezone
from xml.sax.saxutils import escape as xml_escape

import websockets

# ETP type imports
from etpproto.messages import Message
from etptypes.energistics.etp.v12.protocol.core.request_session import RequestSession
from etptypes.energistics.etp.v12.protocol.core.close_session import CloseSession
from etptypes.energistics.etp.v12.protocol.core.ping import Ping
from etptypes.energistics.etp.v12.datatypes.supported_protocol import SupportedProtocol
from etptypes.energistics.etp.v12.datatypes.version import Version
from etptypes.energistics.etp.v12.datatypes.data_value import DataValue

# Dataspace protocol
from etptypes.energistics.etp.v12.protocol.dataspace.get_dataspaces import GetDataspaces
from etptypes.energistics.etp.v12.protocol.dataspace.put_dataspaces import PutDataspaces
from etptypes.energistics.etp.v12.protocol.dataspace.delete_dataspaces import DeleteDataspaces
from etptypes.energistics.etp.v12.datatypes.object.dataspace import Dataspace

# DataArray protocol
from etptypes.energistics.etp.v12.protocol.data_array.get_data_arrays import GetDataArrays
from etptypes.energistics.etp.v12.protocol.data_array.get_data_subarrays import GetDataSubarrays
from etptypes.energistics.etp.v12.protocol.data_array.put_data_arrays import PutDataArrays
from etptypes.energistics.etp.v12.protocol.data_array.put_data_subarrays import PutDataSubarrays
from etptypes.energistics.etp.v12.protocol.data_array.put_uninitialized_data_arrays import PutUninitializedDataArrays
from etptypes.energistics.etp.v12.protocol.data_array.get_data_array_metadata import GetDataArrayMetadata
from etptypes.energistics.etp.v12.datatypes.data_array_types.data_array_identifier import DataArrayIdentifier
from etptypes.energistics.etp.v12.datatypes.data_array_types.put_data_subarrays_type import PutDataSubarraysType
from etptypes.energistics.etp.v12.datatypes.data_array_types.put_data_arrays_type import PutDataArraysType
from etptypes.energistics.etp.v12.datatypes.data_array_types.put_uninitialized_data_array_type import PutUninitializedDataArrayType
from etptypes.energistics.etp.v12.datatypes.data_array_types.data_array_metadata import DataArrayMetadata
from etptypes.energistics.etp.v12.datatypes.data_array_types.data_array import DataArray
from etptypes.energistics.etp.v12.datatypes.data_array_types.get_data_subarrays_type import GetDataSubarraysType
from etptypes.energistics.etp.v12.datatypes.any_array import AnyArray
from etptypes.energistics.etp.v12.datatypes.any_array_type import AnyArrayType
from etptypes.energistics.etp.v12.datatypes.any_logical_array_type import AnyLogicalArrayType
from etptypes.energistics.etp.v12.datatypes.array_of_double import ArrayOfDouble
from etptypes.energistics.etp.v12.datatypes.array_of_float import ArrayOfFloat
from etptypes.energistics.etp.v12.datatypes.array_of_int import ArrayOfInt
from etptypes.energistics.etp.v12.datatypes.array_of_long import ArrayOfLong

# Transaction protocol
from etptypes.energistics.etp.v12.protocol.transaction.start_transaction import StartTransaction
from etptypes.energistics.etp.v12.protocol.transaction.commit_transaction import CommitTransaction
from etptypes.energistics.etp.v12.protocol.transaction.rollback_transaction import RollbackTransaction

# Store protocol
from etptypes.energistics.etp.v12.protocol.store.put_data_objects import PutDataObjects
from etptypes.energistics.etp.v12.protocol.store.get_data_objects import GetDataObjects
from etptypes.energistics.etp.v12.protocol.store.delete_data_objects import DeleteDataObjects
from etptypes.energistics.etp.v12.datatypes.object.data_object import DataObject
from etptypes.energistics.etp.v12.datatypes.object.resource import Resource
from etptypes.energistics.etp.v12.datatypes.object.active_status_kind import ActiveStatusKind

# Discovery protocol
from etptypes.energistics.etp.v12.protocol.discovery.get_resources import GetResources
from etptypes.energistics.etp.v12.datatypes.object.context_info import ContextInfo
from etptypes.energistics.etp.v12.datatypes.object.context_scope_kind import ContextScopeKind

# SupportedTypes protocol
from etptypes.energistics.etp.v12.protocol.supported_types.get_supported_types import GetSupportedTypes

from etptypes.energistics.etp.v12.datatypes.supported_data_object import SupportedDataObject

# ============================================================================
# Wire protocol helpers
# ============================================================================

def build_ssl_context(verify=True):
    """Build SSL context. verify=False disables cert checks (for PPE/C1B)."""
    ctx = ssl.create_default_context()
    if not verify:
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
    return ctx

MSG_ID = [0]  # mutable counter for message IDs (client sends even IDs)

# Build the protocol-to-class mapping for decoding responses
from etpproto.utils import get_all_etp_protocol_classes
PROTO_DICT = get_all_etp_protocol_classes()


def next_msg_id():
    MSG_ID[0] += 2
    return MSG_ID[0]


def encode(msg, msg_id=None):
    """Encode an ETP message to binary frame bytes."""
    if msg_id is None:
        msg_id = next_msg_id()
    m = Message.get_object_message(msg, msg_id=msg_id, correlation_id=0, message_flags=0x02)
    return m.encode_message()


def extract_ascii(raw_bytes):
    """Extract readable ASCII strings from binary data."""
    result = []
    current = []
    for b in raw_bytes:
        if 32 <= b < 127:
            current.append(chr(b))
        else:
            if len(current) >= 4:
                result.append(''.join(current))
            current = []
    if len(current) >= 4:
        result.append(''.join(current))
    return ' | '.join(result)


def decode(frame_bytes):
    """Decode a binary ETP frame to a message object."""
    try:
        m = Message.decode_binary_message(frame_bytes, PROTO_DICT)
        if m and m.body:
            return m.body
        return {"_ascii": extract_ascii(frame_bytes)}
    except Exception:
        return {"_ascii": extract_ascii(frame_bytes)}


def build_request_session():
    """Build a RequestSession message."""
    return RequestSession(
        application_name="etp-test-client",
        application_version="1.0",
        client_instance_id=uuid.uuid4(),
        requested_protocols=[
            SupportedProtocol(protocol=3, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
            SupportedProtocol(protocol=4, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
            SupportedProtocol(protocol=9, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
            SupportedProtocol(protocol=18, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
            SupportedProtocol(protocol=24, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
            SupportedProtocol(protocol=25, protocol_version=Version(major=1, minor=2, revision=0, patch=0), role="store", protocol_capabilities={}),
        ],
        supported_data_objects=[
            SupportedDataObject(qualified_type="eml20.*", data_object_capabilities={}),
            SupportedDataObject(qualified_type="resqml20.*", data_object_capabilities={}),
            SupportedDataObject(qualified_type="resqml22.*", data_object_capabilities={}),
            SupportedDataObject(qualified_type="eml23.*", data_object_capabilities={}),
        ],
        supported_compression=["xml"],
        supported_formats=["xml"],
        current_date_time=0,
        earliest_retained_change_time=0,
        endpoint_capabilities={},
    )


# ============================================================================
# Command implementations
# ============================================================================

async def recv_responses(ws, timeout=10, max_msgs=6):
    """Receive all response messages from the server.

    Uses 'timeout' for the first frame, then a shorter 3s window for
    subsequent frames (servers send multi-frame responses in quick bursts).
    """
    responses = []
    for _ in range(max_msgs):
        try:
            data = await asyncio.wait_for(ws.recv(), timeout=timeout if not responses else 3.0)
            responses.append(data)
        except asyncio.TimeoutError:
            break
    return responses


def print_response(label, raw_frames):
    """Print decoded ETP responses in human-readable format."""
    print(f"\n  << {label} ({len(raw_frames)} frame(s)):")
    for i, frame in enumerate(raw_frames):
        if isinstance(frame, bytes):
            decoded = decode(frame)
            if isinstance(decoded, dict) and "_ascii" in decoded:
                print(f"     [{i}] (raw text): {decoded['_ascii']}")
            else:
                msg_type = type(decoded).__name__
                print(f"     [{i}] {msg_type}")
                try:
                    d = decoded.__dict__ if hasattr(decoded, '__dict__') else {}
                    for k, v in d.items():
                        if not k.startswith('_'):
                            if isinstance(v, (list, tuple)) and len(v) > 0:
                                print(f"         {k}: ({len(v)} items)")
                                for j, item in enumerate(v):
                                    print(f"           [{j}] {item}")
                            elif isinstance(v, dict) and len(v) > 0:
                                print(f"         {k}: ({len(v)} entries)")
                                for dk, dv in v.items():
                                    print(f"           '{dk}': {dv}")
                            else:
                                print(f"         {k}: {v}")
                except Exception:
                    print(f"         (could not format)")
        else:
            print(f"     [{i}] Text: {str(frame)[:200]}")


async def cmd_ping(ws):
    """Send a Ping and wait for Pong."""
    print("\n  >> Ping")
    await ws.send(encode(Ping(current_date_time=0)))
    responses = await recv_responses(ws, timeout=5, max_msgs=1)
    print_response("Pong", responses)


async def cmd_get_dataspaces(ws):
    """Get all dataspaces."""
    print("\n  >> GetDataspaces")
    await ws.send(encode(GetDataspaces(store_last_write_filter=None)))
    responses = await recv_responses(ws)
    print_response("GetDataspacesResponse", responses)


async def cmd_put_dataspace(ws, args):
    """Create a new dataspace."""
    path = args.path
    legaltag = args.legaltag
    partition = args.partition
    viewers = args.viewers or f"data.default.viewers@{partition}.dataservices.energy"
    owners = args.owners or f"data.default.owners@{partition}.dataservices.energy"
    countries = args.countries

    ds = Dataspace(
        uri=f"eml:///dataspace('{path}')",
        path=path,
        store_last_write=0,
        store_created=0,
        custom_data={
            "legaltags": DataValue(item=legaltag),
            "otherRelevantDataCountries": DataValue(item=countries),
            "viewers": DataValue(item=viewers),
            "owners": DataValue(item=owners),
        }
    )
    print(f"\n  >> PutDataspaces (path='{path}', legaltag='{legaltag}')")
    print(f"     viewers: {viewers}")
    print(f"     owners: {owners}")
    print(f"     countries: {countries}")
    await ws.send(encode(PutDataspaces(dataspaces={path: ds})))
    responses = await recv_responses(ws)
    print_response("PutDataspacesResponse", responses)


async def cmd_delete_dataspace(ws, args):
    """Delete a dataspace."""
    uri = f"eml:///dataspace('{args.path}')"
    print(f"\n  >> DeleteDataspaces (uri='{uri}')")
    await ws.send(encode(DeleteDataspaces(uris={args.path: uri})))
    responses = await recv_responses(ws)
    print_response("DeleteDataspacesResponse", responses)


async def cmd_put_data_object(ws, args):
    """Create an EpcExternalPartReference object (container for arrays)."""
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    name = args.name or f"ext_part_{obj_uuid[:8]}"
    safe_name = xml_escape(name)

    # Minimal EpcExternalPartReference XML
    xml_data = f"""<?xml version="1.0" encoding="utf-8"?>
<EpcExternalPartReference xmlns="http://www.energistics.org/energyml/data/commonv2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" schemaVersion="2.2" uuid="{obj_uuid}" xsi:type="EpcExternalPartReference">
  <Citation>
    <Title>{safe_name}</Title>
    <Originator>etp-test-client</Originator>
    <Creation>{datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}</Creation>
    <Format>Energistics:etp12</Format>
  </Citation>
  <Filename>{safe_name}.epc</Filename>
</EpcExternalPartReference>"""

    resource = Resource(
        uri=uri,
        name=name,
        source_count=0,
        target_count=0,
        last_changed=0,
        store_last_write=0,
        store_created=0,
        active_status=ActiveStatusKind.ACTIVE,
        alternate_uris=[],
        custom_data={},
    )
    data_obj = DataObject(
        resource=resource,
        blob_id=None,
        format_="xml",
        data=xml_data.encode("utf-8"),
    )
    msg = PutDataObjects(data_objects={"obj0": data_obj})

    print(f"\n  >> PutDataObjects (EpcExternalPartReference)")
    print(f"     URI: {uri}")
    print(f"     Name: {name}")
    print(f"     UUID: {obj_uuid}")
    await ws.send(encode(msg))
    responses = await recv_responses(ws)
    print_response("PutDataObjectsResponse", responses)
    return obj_uuid


async def cmd_put_uninit_array(ws, args):
    """Put an uninitialized (empty) array."""
    dims = [int(x) for x in args.dims.split(",")]
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    transport_type_map = {
        "double": AnyArrayType.ARRAY_OF_DOUBLE,
        "float": AnyArrayType.ARRAY_OF_FLOAT,
        "int": AnyArrayType.ARRAY_OF_INT,
        "long": AnyArrayType.ARRAY_OF_LONG,
    }
    logical_type_map = {
        "double": AnyLogicalArrayType.ARRAY_OF_DOUBLE64_LE,
        "float": AnyLogicalArrayType.ARRAY_OF_FLOAT32_LE,
        "int": AnyLogicalArrayType.ARRAY_OF_INT32_LE,
        "long": AnyLogicalArrayType.ARRAY_OF_INT64_LE,
    }
    transport_type = transport_type_map.get(args.type, AnyArrayType.ARRAY_OF_DOUBLE)
    logical_type = logical_type_map.get(args.type, AnyLogicalArrayType.ARRAY_OF_DOUBLE64_LE)

    msg = PutUninitializedDataArrays(data_arrays={"a0": PutUninitializedDataArrayType(
        uid=ident,
        metadata=DataArrayMetadata(
            dimensions=dims,
            transport_array_type=transport_type,
            logical_array_type=logical_type,
            store_last_write=0,
            store_created=0,
            preferred_subarray_dimensions=[],
            custom_data={},
        )
    )})
    print(f"\n  >> PutUninitializedDataArrays (dims={dims}, type={args.type}, path='{args.path}')")
    print(f"     URI: {uri}")
    await ws.send(encode(msg))
    responses = await recv_responses(ws)
    print_response("PutUninitializedDataArraysResponse", responses)
    return ident  # return for use in subsequent commands


async def cmd_put_subarrays(ws, args):
    """Put data into a sub-region of an existing array."""
    counts = [int(x) for x in args.counts.split(",")]
    starts = [int(x) for x in args.starts.split(",")]
    data = [float(x) for x in args.data.split(",")]

    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    msg = PutDataSubarrays(data_subarrays={"p": PutDataSubarraysType(
        uid=ident,
        data=AnyArray(item=ArrayOfDouble(values=data)),
        starts=starts,
        counts=counts,
    )})
    print(f"\n  >> PutDataSubarrays (counts={counts}, starts={starts}, data_len={len(data)})")
    print(f"     URI: {uri}")
    await ws.send(encode(msg))
    responses = await recv_responses(ws)
    print_response("PutDataSubarraysResponse", responses)


async def cmd_get_arrays(ws, args):
    """Get a full array."""
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    print(f"\n  >> GetDataArrays (path='{args.path}')")
    print(f"     URI: {uri}")
    await ws.send(encode(GetDataArrays(data_arrays={"g": ident})))
    responses = await recv_responses(ws)
    print_response("GetDataArraysResponse", responses)


async def cmd_get_subarrays(ws, args):
    """Get a sub-region of an array."""
    counts = [int(x) for x in args.counts.split(",")]
    starts = [int(x) for x in args.starts.split(",")]

    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    msg = GetDataSubarrays(data_subarrays={"s": GetDataSubarraysType(
        uid=ident, starts=starts, counts=counts
    )})
    print(f"\n  >> GetDataSubarrays (starts={starts}, counts={counts})")
    await ws.send(encode(msg))
    responses = await recv_responses(ws)
    print_response("GetDataSubarraysResponse", responses)


async def cmd_put_arrays(ws, args):
    """Put a data array (create + fill in one shot)."""
    dims = [int(x) for x in args.dims.split(",")]
    data = [float(x) for x in args.data.split(",")]
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    msg = PutDataArrays(data_arrays={"a0": PutDataArraysType(
        uid=ident,
        array=DataArray(
            dimensions=dims,
            data=AnyArray(item=ArrayOfDouble(values=data)),
        ),
        custom_data={},
    )})
    print(f"\n  >> PutDataArrays (dims={dims}, data_len={len(data)}, path='{args.path}')")
    print(f"     URI: {uri}")
    await ws.send(encode(msg))
    responses = await recv_responses(ws)
    print_response("PutDataArraysResponse", responses)


async def cmd_get_data_objects(ws, args):
    """Get data objects from the Store."""
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    print(f"\n  >> GetDataObjects")
    print(f"     URI: {uri}")
    await ws.send(encode(GetDataObjects(uris={"o0": uri}, format_="xml")))
    responses = await recv_responses(ws)
    print_response("GetDataObjectsResponse", responses)


async def cmd_delete_data_objects(ws, args):
    """Delete data objects from the Store."""
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    print(f"\n  >> DeleteDataObjects")
    print(f"     URI: {uri}")
    await ws.send(encode(DeleteDataObjects(uris={"d0": uri}, prune_contained_objects=True)))
    responses = await recv_responses(ws)
    print_response("DeleteDataObjectsResponse", responses)


async def cmd_get_metadata(ws, args):
    """Get array metadata."""
    obj_uuid = args.obj_uuid if args.obj_uuid else str(uuid.uuid4())
    uri = f"eml:///dataspace('{args.dataspace}')/resqml22.EpcExternalPartReference({obj_uuid})"
    ident = DataArrayIdentifier(uri=uri, path_in_resource=args.path)

    print(f"\n  >> GetDataArrayMetadata (path='{args.path}')")
    await ws.send(encode(GetDataArrayMetadata(data_arrays={"m": ident})))
    responses = await recv_responses(ws)
    print_response("GetDataArrayMetadataResponse", responses)


async def cmd_start_transaction(ws, args):
    """Start a transaction."""
    dataspaces = []
    if args.dataspace:
        uri = f"eml:///dataspace('{args.dataspace}')"
        dataspaces = [uri]
        print(f"\n  >> StartTransaction (dataspace='{args.dataspace}', uri='{uri}')")
    else:
        print(f"\n  >> StartTransaction (no dataspace scope)")
    await ws.send(encode(StartTransaction(read_only=False, message="test", dataspace_uris=dataspaces)))
    responses = await recv_responses(ws)
    print_response("StartTransactionResponse", responses)


async def cmd_commit_transaction(ws, args):
    """Commit a transaction."""
    txn_uuid = args.txn_uuid
    print(f"\n  >> CommitTransaction (uuid='{txn_uuid}')")
    await ws.send(encode(CommitTransaction(transaction_uuid=txn_uuid)))
    responses = await recv_responses(ws)
    print_response("CommitTransactionResponse", responses)


async def cmd_rollback_transaction(ws, args):
    """Rollback a transaction."""
    txn_uuid = args.txn_uuid
    print(f"\n  >> RollbackTransaction (uuid='{txn_uuid}')")
    await ws.send(encode(RollbackTransaction(transaction_uuid=txn_uuid)))
    responses = await recv_responses(ws)
    print_response("RollbackTransactionResponse", responses)


async def cmd_get_supported_types(ws):
    """Get supported types."""
    print("\n  >> GetSupportedTypes")
    await ws.send(encode(GetSupportedTypes(uri="eml:///", count_objects=False, return_empty_types=True, scope=ContextScopeKind.SELF)))
    responses = await recv_responses(ws)
    print_response("GetSupportedTypesResponse", responses)


async def cmd_get_resources(ws, args):
    """Get resources (Discovery)."""
    uri = f"eml:///dataspace('{args.dataspace}')"
    print(f"\n  >> GetResources (dataspace='{args.dataspace}')")
    ctx = ContextInfo(
        uri=uri, depth=1, data_object_types=[],
        navigable_edges="Primary",
        include_secondary_edges=False,
        include_non_navigable_edges=False,
    )
    await ws.send(encode(GetResources(context=ctx, scope=ContextScopeKind.SELF, count_objects=False, store_last_write_filter=None, active_status_filter=None, include_edges=False)))
    responses = await recv_responses(ws)
    print_response("GetResourcesResponse", responses)


# ============================================================================
# Main
# ============================================================================

COMMANDS_HELP = """\
  Commands:
    ping                          Send Ping, get Pong
    get-dataspaces                List all dataspaces
    put-dataspace                 Create a dataspace (--path, --legaltag)
    delete-dataspace              Delete a dataspace (--path)
    put-data-object               Create EpcExternalPartReference (--dataspace, [--obj-uuid], [--name])
    get-data-objects              Get a data object (--dataspace, --obj-uuid)
    delete-data-objects           Delete a data object (--dataspace, --obj-uuid)
    put-uninit-array              Create empty array (--dataspace, --obj-uuid, --dims, --path, [--type])
    put-arrays                    Create+fill array (--dataspace, --obj-uuid, --dims, --path, --data)
    put-subarrays                 Write sub-region (--dataspace, --obj-uuid, --path, --starts, --counts, --data)
    get-arrays                    Read full array (--dataspace, --obj-uuid, --path)
    get-subarrays                 Read sub-region (--dataspace, --obj-uuid, --path, --starts, --counts)
    get-metadata                  Get array metadata (--dataspace, --obj-uuid, --path)
    start-transaction             Start transaction (--dataspace)
    commit-transaction            Commit (--txn-uuid)
    rollback-transaction          Rollback (--txn-uuid)
    get-supported-types           List supported types
    get-resources                 Discovery (--dataspace)
    help                          Show this help
    exit / quit / close           Close session and exit
"""


async def dispatch_command(ws, args):
    """Dispatch a single command on the open session."""
    cmd = args.command
    if cmd == "ping":
        await cmd_ping(ws)
    elif cmd == "get-dataspaces":
        await cmd_get_dataspaces(ws)
    elif cmd == "put-dataspace":
        await cmd_put_dataspace(ws, args)
    elif cmd == "delete-dataspace":
        await cmd_delete_dataspace(ws, args)
    elif cmd == "put-data-object":
        await cmd_put_data_object(ws, args)
    elif cmd == "put-uninit-array":
        await cmd_put_uninit_array(ws, args)
    elif cmd == "put-subarrays":
        await cmd_put_subarrays(ws, args)
    elif cmd == "get-arrays":
        await cmd_get_arrays(ws, args)
    elif cmd == "get-subarrays":
        await cmd_get_subarrays(ws, args)
    elif cmd == "put-arrays":
        await cmd_put_arrays(ws, args)
    elif cmd == "get-data-objects":
        await cmd_get_data_objects(ws, args)
    elif cmd == "delete-data-objects":
        await cmd_delete_data_objects(ws, args)
    elif cmd == "get-metadata":
        await cmd_get_metadata(ws, args)
    elif cmd == "start-transaction":
        await cmd_start_transaction(ws, args)
    elif cmd == "commit-transaction":
        await cmd_commit_transaction(ws, args)
    elif cmd == "rollback-transaction":
        await cmd_rollback_transaction(ws, args)
    elif cmd == "get-supported-types":
        await cmd_get_supported_types(ws)
    elif cmd == "get-resources":
        await cmd_get_resources(ws, args)
    elif cmd == "help":
        print(COMMANDS_HELP)
    elif cmd in ("exit", "quit", "close"):
        return False  # signal to exit
    else:
        print(f"\n  ERROR: Unknown command '{cmd}'. Type 'help' for available commands.")
    return True  # keep going


def parse_interactive_line(line, base_args):
    """Parse an interactive command line into an args namespace."""
    import shlex
    try:
        parts = shlex.split(line)
    except ValueError:
        parts = line.split()
    if not parts:
        return None

    # Build a mini parser for interactive commands
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("command")
    parser.add_argument("--path", default=base_args.path)
    parser.add_argument("--legaltag", default=base_args.legaltag)
    parser.add_argument("--viewers", default=base_args.viewers)
    parser.add_argument("--owners", default=base_args.owners)
    parser.add_argument("--countries", default=base_args.countries)
    parser.add_argument("--dataspace", default=base_args.dataspace)
    parser.add_argument("--name", default=base_args.name)
    parser.add_argument("--dims", default=base_args.dims)
    parser.add_argument("--type", default=base_args.type)
    parser.add_argument("--counts", default=base_args.counts)
    parser.add_argument("--starts", default=base_args.starts)
    parser.add_argument("--data", default=base_args.data)
    parser.add_argument("--obj-uuid", dest="obj_uuid", default=base_args.obj_uuid)
    parser.add_argument("--txn-uuid", dest="txn_uuid", default=base_args.txn_uuid)
    parser.add_argument("--partition", default=base_args.partition)

    try:
        return parser.parse_args(parts)
    except SystemExit:
        return None


async def run(args):
    """Main async entry point — connects and dispatches commands."""
    endpoint = args.url if getattr(args, "url", None) else f"wss://{args.dns}/api/reservoir-ddms-etp/v2/"
    use_tls = endpoint.startswith("wss://")
    headers = {
        "Authorization": f"Bearer {args.token}",
        "data-partition-id": args.partition,
    }

    interactive = (args.command == "interactive")

    print(f"=== ETP 1.2 Test Client ===")
    print(f"  Endpoint: {endpoint}")
    print(f"  Partition: {args.partition}")
    print(f"  Mode: {'interactive (Ctrl+C to exit)' if interactive else args.command}")

    async with websockets.connect(
        endpoint,
        additional_headers=headers,
        subprotocols=["etp12.energistics.org"],
        ssl=build_ssl_context(verify=not args.insecure) if use_tls else None,
        open_timeout=20,
        max_size=None,
        ping_interval=None,
        ping_timeout=None,
    ) as ws:
        # Always start with RequestSession
        print("\n  >> RequestSession")
        await ws.send(encode(build_request_session()))
        session_resp = await recv_responses(ws, timeout=10, max_msgs=1)
        if not session_resp:
            print("\n  *** ERROR: No response to RequestSession. Server may have rejected the connection.")
            return
        print_response("OpenSession", session_resp)

        if interactive:
            # Interactive REPL — session stays open
            print("\n  Session open. Type commands (e.g. 'get-dataspaces', 'help', 'exit').")
            print("  Press Ctrl+C to close.\n")
            try:
                while True:
                    try:
                        line = input("etp> ").strip()
                    except EOFError:
                        break
                    if not line:
                        continue
                    cmd_args = parse_interactive_line(line, args)
                    if cmd_args is None:
                        print("  (could not parse command)")
                        continue
                    try:
                        keep_going = await dispatch_command(ws, cmd_args)
                        if not keep_going:
                            break
                    except websockets.exceptions.ConnectionClosedError:
                        print("\n  ERROR: Server disconnected (idle timeout). Restart the session.")
                        break
                    except websockets.exceptions.ConnectionClosed:
                        print("\n  ERROR: Connection closed by server. Restart the session.")
                        break
            except KeyboardInterrupt:
                print("\n\n  Ctrl+C received.")

            # Graceful close (if still connected)
            try:
                print("  >> CloseSession")
                await ws.send(encode(CloseSession(reason="user exit")))
                responses = await recv_responses(ws, timeout=3, max_msgs=1)
                print_response("CloseSession ack", responses)
            except Exception:
                pass
        else:
            # Single-command mode (original behavior)
            await dispatch_command(ws, args)

    print("\n=== Done ===")


def main():
    parser = argparse.ArgumentParser(
        description="ETP 1.2 Test Client for OSDU Reservoir DDMS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--dns", required=False, help="ADME instance DNS (e.g. instance.energy.azure.com)")
    parser.add_argument("--url", help="Full WebSocket endpoint override, e.g. ws://oetp-server-service.ddms-reservoir:80/ (plain ws:// skips TLS). Takes precedence over --dns.")
    parser.add_argument("--token", help="Bearer token for authentication (or use --token-file)")
    parser.add_argument("--token-file", dest="token_file", help="Path to file containing the bearer token (one line, no newline)")
    parser.add_argument("--insecure", action="store_true", help="Disable TLS certificate verification (needed for PPE/C1B instances with self-signed certs)")
    parser.add_argument("--partition", default="dp1", help="Data partition ID (default: dp1)")
    parser.add_argument("command", help="Command to execute, or 'interactive' for REPL mode")

    # Command-specific arguments
    parser.add_argument("--path", help="Dataspace path or array path in resource")
    parser.add_argument("--legaltag", help="Legal tag name for dataspace creation")
    parser.add_argument("--viewers", help="Viewers ACL group (default: data.default.viewers@<partition>.dataservices.energy)")
    parser.add_argument("--owners", help="Owners ACL group (default: data.default.owners@<partition>.dataservices.energy)")
    parser.add_argument("--countries", default="US", help="Comma-separated country codes (default: US)")
    parser.add_argument("--dataspace", help="Dataspace path for array operations")
    parser.add_argument("--name", help="Object name (for put-data-object)")
    parser.add_argument("--dims", help="Array dimensions (comma-separated, e.g. 4,4)")
    parser.add_argument("--type", default="double", help="Array element type (double, float, int, long)")
    parser.add_argument("--counts", help="Sub-array counts (comma-separated)")
    parser.add_argument("--starts", help="Sub-array starts (comma-separated)")
    parser.add_argument("--data", help="Array data values (comma-separated doubles)")
    parser.add_argument("--obj-uuid", dest="obj_uuid", help="Object UUID (auto-generated if not provided)")
    parser.add_argument("--txn-uuid", dest="txn_uuid", help="Transaction UUID (from start-transaction response)")

    args = parser.parse_args()

    # Resolve token: --token-file takes precedence, then --token
    if args.token_file:
        with open(args.token_file) as f:
            args.token = f.read().strip()
    if not args.token:
        parser.error("Either --token or --token-file is required")

    asyncio.run(run(args))


if __name__ == "__main__":
    import platform
    if platform.system() == 'Windows':
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    main()
