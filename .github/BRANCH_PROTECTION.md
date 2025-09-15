# GitHub分支保护规则配置

为了确保所有代码在merge前都通过单元测试，建议配置以下分支保护规则：

## 配置步骤

1. 进入 GitHub 仓库页面
2. 点击 **Settings** 标签
3. 在左侧菜单中选择 **Branches**
4. 点击 **Add rule** 或编辑现有规则

## 推荐的分支保护设置

### 对于 main 分支：
- ✅ **Require status checks to pass before merging**
  - ✅ **Require branches to be up to date before merging** 
  - 在 **Status checks** 中添加: `test`
- ✅ **Require pull request reviews before merging**
  - **Required number of reviews**: 1
- ✅ **Dismiss stale reviews when new commits are pushed**
- ✅ **Require review from code owners** (如果有 CODEOWNERS 文件)
- ✅ **Include administrators** (让管理员也遵循相同规则)

### 对于 dev 分支：
- ✅ **Require status checks to pass before merging**
  - ✅ **Require branches to be up to date before merging**
  - 在 **Status checks** 中添加: `test`
- ✅ **Include administrators**

## 说明

- `test` 是在 workflow 文件中定义的 job 名称
- 这些设置将确保：
  - 所有 pull request 必须通过单元测试才能合并
  - 分支必须是最新的（基于目标分支）
  - main 分支需要代码审查
  - 管理员也需要遵循相同规则

配置完成后，任何向 main 或 dev 分支的 pull request 都必须通过单元测试才能被合并。