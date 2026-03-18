<#
.SYNOPSIS
    V340L MxGPU DDA Survey Tool (Day 1 Reconnaissance)

.DESCRIPTION
    Scans the Windows PCIe topology for AMD V340L components (PF DEV_6864, VF DEV_686C) 
    and the embedded Microsemi Switchtec PSX management endpoint (VEN_11F8).
    
    Extracts the BDF (Bus:Device.Function) and the PCIROOT Location Paths strictly 
    required for Hyper-V Discrete Device Assignment (DDA).

.NOTES
    Windows does not expose raw Linux-style IOMMU groups (/sys/kernel/iommu_groups). 
    Hyper-V DDA relies on ACS (Access Control Services) at the BIOS level and uses 
    the Location Path to map isolated devices into VMs.
#>

[CmdletBinding()]
param()

# 1. Enforce Administrator Privileges (Required for DEVPKEY extraction)
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Warning "CRITICAL: Please run this script as Administrator."
    Write-Warning "Extraction of PCI Location Paths for DDA requires elevated privileges."
    exit
}

Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host " V340L MxGPU DDA Survey Tool (Day 1 Reconnaissance)" -ForegroundColor Cyan
Write-Host "===============================================================" -ForegroundColor Cyan
Write-Host "Scanning for AMD (VEN_1002) and Switchtec (VEN_11F8)...`n"

# 2. Target Specific Vendors to avoid noise
$targetVendors = @("VEN_1002", "VEN_11F8")

$devices = Get-PnpDevice -PresentOnly | Where-Object {
    $hwids = $_.HardwareID
    $match = $false
    if ($hwids) {
        foreach ($vid in $targetVendors) {
            if ($hwids -match $vid) { $match = $true; break }
        }
    }
    $match
}

$results = @()

# 3. Iterate and Extract Low-Level PCIe Properties
foreach ($dev in $devices) {
    $props = Get-PnpDeviceProperty -InstanceId $dev.InstanceId

    # A. Extract Location Path (required for DDA Dismount/Assign)
    $locationPathsProp = $props | Where-Object KeyName -eq "DEVPKEY_Device_LocationPaths"
    $locationPath = if ($locationPathsProp.Data) { 
        ($locationPathsProp.Data | Where-Object { $_ -match "^PCIROOT" }) -join ", " 
    } else { 
        "N/A" 
    }

    # B. Extract BDF (Bus:Device.Function)
    $busProp = $props | Where-Object KeyName -eq "DEVPKEY_Device_BusNumber"
    $bus = if ($null -ne $busProp.Data) { $busProp.Data } else { "N/A" }

    $addrProp = $props | Where-Object KeyName -eq "DEVPKEY_Device_Address"
    if ($null -ne $addrProp.Data) {
        $deviceNum = [math]::Truncate($addrProp.Data / 65536)
        $funcNum = $addrProp.Data % 65536
        $bdf = "$bus`:$deviceNum.$funcNum"
    } else {
        $bdf = "N/A"
    }

    # C. Classify component
    $role = "Unknown"
    $primaryId = ($dev.HardwareID | Select-Object -First 1)

    if ($primaryId -match "DEV_6864") { 
        $role = "V340L PF (Die)" 
    } elseif ($primaryId -match "DEV_686C") { 
        $role = "V340L VF (MxGPU)" 
    } elseif ($primaryId -match "VEN_11F8") { 
        $role = "Switchtec Fabric" 
    } else {
        $role = "Other AMD Device"
    }

    $results += [PSCustomObject]@{
        Role         = $role
        BDF          = $bdf
        Status       = $dev.Status
        Name         = $dev.FriendlyName
        HardwareID   = $primaryId
        LocationPath = $locationPath
    }
}

# 4. Output
if ($results.Count -eq 0) {
    Write-Host "[!] No AMD or Switchtec devices found. Check external power and riser seating." -ForegroundColor Red
} else {
    $results | Sort-Object Role, BDF | Format-Table -AutoSize
    
    $pfCount = ($results | Where-Object Role -eq "V340L PF (Die)").Count
    $swCount = ($results | Where-Object Role -eq "Switchtec Fabric").Count
    
    Write-Host "===============================================================" -ForegroundColor Cyan
    Write-Host " Day 1 Hardware Gate Status:" -ForegroundColor Cyan
    
    if ($pfCount -eq 2 -and $swCount -ge 1) {
        Write-Host " [PASS] 2x PFs and 1x Switchtec enumerated. Topology is sound." -ForegroundColor Green
    } else {
        Write-Host " [FAIL] Expected 2x PFs and 1x Switchtec. Found $pfCount PF(s) and $swCount Switchtec(s)." -ForegroundColor Red
    }

    Write-Host "`n===============================================================" -ForegroundColor Cyan
    Write-Host " Hyper-V DDA Commands (Path A / Path B)" -ForegroundColor Cyan
    Write-Host "===============================================================" -ForegroundColor Cyan
    
    $ddaGenerated = $false
    foreach ($res in $results) {
        if ($res.LocationPath -ne "N/A" -and ($res.Role -match "V340L PF" -or $res.Role -match "V340L VF")) {
            Write-Host ("# Dismount " + $res.Role + " at " + $res.BDF) -ForegroundColor Gray
            Write-Host ("Dismount-VMHostAssignableDevice -LocationPath `"" + $res.LocationPath + "`" -Force") -ForegroundColor Yellow
            Write-Host ("Add-VMAssignableDevice -VMName `"YOUR_VM_NAME`" -LocationPath `"" + $res.LocationPath + "`"`n") -ForegroundColor Yellow
            $ddaGenerated = $true
        }
    }

    if (-not $ddaGenerated) {
        Write-Host " No assignable Location Paths found. Ensure SR-IOV, VT-d, and ACS are enabled in BIOS." -ForegroundColor Red
    }
}
