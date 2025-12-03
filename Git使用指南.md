# Git 使用指南

## 拉取最新代码

### 基本命令

```bash
# 拉取最新代码（推荐）
git pull origin main

# 或者分两步执行
git fetch origin        # 获取远程更新
git merge origin/main   # 合并到本地
```

### 完整操作流程

#### 1. 首次克隆项目

```bash
# 克隆仓库
git clone https://github.com/Jayus0/frame.git

# 进入项目目录
cd frame

# 确认在 main 分支
git checkout main
```

#### 2. 日常更新代码

```bash
# 进入项目目录
cd frame

# 拉取最新代码
git pull origin main
```

#### 3. 如果本地有修改

```bash
# 方式1：先提交本地修改，再拉取
git add .
git commit -m "本地修改说明"
git pull origin main

# 方式2：暂存本地修改，拉取后再恢复
git stash              # 暂存本地修改
git pull origin main   # 拉取最新代码
git stash pop          # 恢复本地修改

# 方式3：查看本地修改，决定是否保留
git status             # 查看修改状态
git diff               # 查看具体修改内容
```

### 常用 Git 命令

#### 查看状态
```bash
# 查看当前状态
git status

# 查看远程仓库信息
git remote -v

# 查看所有分支
git branch -a

# 查看提交历史
git log --oneline
```

#### 分支操作
```bash
# 切换到 main 分支
git checkout main

# 创建新分支
git checkout -b feature/new-feature

# 查看当前分支
git branch
```

#### 更新操作
```bash
# 拉取最新代码（最常用）
git pull origin main

# 获取远程更新但不合并
git fetch origin

# 查看远程更新
git fetch origin
git log HEAD..origin/main

# 强制拉取（会覆盖本地修改，谨慎使用）
git fetch origin
git reset --hard origin/main
```

### 解决冲突

如果拉取时出现冲突：

```bash
# 1. 拉取代码
git pull origin main

# 2. 如果有冲突，Git 会提示
# 打开冲突文件，手动解决冲突
# 冲突标记：
# <<<<<<< HEAD
# 你的代码
# =======
# 远程代码
# >>>>>>> origin/main

# 3. 解决冲突后
git add <冲突文件>
git commit -m "解决合并冲突"
```

### 推荐工作流程

#### 每天开始工作前
```bash
cd frame
git checkout main
git pull origin main
```

#### 提交代码前
```bash
# 1. 先拉取最新代码
git pull origin main

# 2. 解决可能的冲突

# 3. 添加修改
git add .

# 4. 提交
git commit -m "修改说明"

# 5. 推送
git push origin main
```

### 使用 Qt Creator 拉取代码

Qt Creator 也支持 Git 操作：

1. **菜单方式**：
   - 工具 → Git → Pull
   - 或者：工具 → Git → Fetch，然后 Merge

2. **版本控制面板**：
   - 左侧边栏 → 版本控制
   - 右键点击 → Pull

### 快速参考

```bash
# 最常用的三个命令
git status          # 查看状态
git pull origin main # 拉取最新代码
git push origin main # 推送代码
```

### 注意事项

⚠️ **重要提示**：

1. **拉取前先提交本地修改**，避免冲突
2. **不要强制拉取**（`git pull --force`），除非确定要丢弃本地修改
3. **定期拉取**，保持代码同步
4. **遇到冲突时**，仔细检查，不要盲目覆盖

### 问题排查

#### 问题1：提示 "Your branch is behind"
```bash
# 说明本地代码落后，需要拉取
git pull origin main
```

#### 问题2：提示 "Your branch has diverged"
```bash
# 说明本地和远程都有新提交
git pull origin main --rebase  # 使用 rebase 方式合并
# 或者
git pull origin main           # 使用 merge 方式合并
```

#### 问题3：提示 "Please commit your changes"
```bash
# 先提交或暂存本地修改
git add .
git commit -m "临时提交"
# 或者
git stash
```
