import os
import sys
import shutil
import zipfile

def main():
    print("=== Windows Packaging Script ===")
    workspace = os.environ.get('GITHUB_WORKSPACE', os.getcwd())
    tbuild = os.environ.get('TBUILD', os.path.join(workspace, 'TBuild'))
    repo_name = os.environ.get('REPO_NAME', 'my-tdesktop-boost')
    
    candidate_repo_dirs = [
        os.path.join(tbuild, repo_name),
        os.path.join(workspace, repo_name),
        workspace,
        os.getcwd()
    ]
    repo_dir = None
    for d in candidate_repo_dirs:
        if os.path.isfile(os.path.join(d, '新手使用指南.txt')) or os.path.isfile(os.path.join(d, 'update.bat')):
            repo_dir = d
            break
            
    print(f"Workspace: {workspace}")
    print(f"Repo Dir: {repo_dir}")
    
    artifact_dir = os.path.join(workspace, 'artifact')
    os.makedirs(artifact_dir, exist_ok=True)
    
    # 1. Check binaries (Telegram.exe and Updater.exe)
    binaries = ['Telegram.exe', 'Updater.exe']
    possible_out_dirs = [
        artifact_dir,
        os.path.join(tbuild, repo_name, 'out', 'Release'),
        os.path.join(workspace, 'out', 'Release')
    ]
    for b in binaries:
        dst = os.path.join(artifact_dir, b)
        if not os.path.isfile(dst):
            for d in possible_out_dirs:
                src = os.path.join(d, b)
                if os.path.isfile(src):
                    print(f"Copying {b} from {d} -> {artifact_dir}")
                    shutil.copy2(src, dst)
                    break
        if os.path.isfile(dst):
            print(f"Found binary: {dst} ({os.path.getsize(dst):,} bytes)")
        else:
            print(f"WARNING: Binary {b} not found yet in {artifact_dir} or out dirs!")

    # 2. Copy user-facing docs and scripts
    user_files = ['新手使用指南.txt', '一键更新.bat', 'update.bat', 'update.ps1', 'README.md']
    if repo_dir:
        for fname in user_files:
            src = os.path.join(repo_dir, fname)
            if os.path.isfile(src):
                dst = os.path.join(artifact_dir, fname)
                print(f"Copying user asset: {fname} -> {dst}")
                shutil.copy2(src, dst)
        # Also copy 新手使用指南.txt to QuickStart_Guide.txt for non-ASCII safety on Release upload
        guide_src = os.path.join(repo_dir, '新手使用指南.txt')
        if os.path.isfile(guide_src):
            shutil.copy2(guide_src, os.path.join(artifact_dir, 'QuickStart_Guide.txt'))
            print("Copied QuickStart_Guide.txt")

    # 3. Create Telegram_x64_qt6.zip
    zip_path = os.path.join(artifact_dir, 'Telegram_x64_qt6.zip')
    print(f"Creating zip package: {zip_path}")
    
    files_to_zip = []
    for fname in os.listdir(artifact_dir):
        if fname.lower().endswith('.zip'):
            continue
        fpath = os.path.join(artifact_dir, fname)
        if os.path.isfile(fpath):
            files_to_zip.append(fname)
            
    print(f"Files to include in zip ({len(files_to_zip)}): {files_to_zip}")
    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for fname in files_to_zip:
            fpath = os.path.join(artifact_dir, fname)
            print(f"  Adding: {fname} ({os.path.getsize(fpath):,} bytes)...")
            zf.write(fpath, arcname=fname)
            
    print(f"Zip created successfully! Size: {os.path.getsize(zip_path):,} bytes")
    print("=== Packaging complete ===")

if __name__ == '__main__':
    main()
