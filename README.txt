1. Generate 2 Bitfree addresses, one for mortgage, one for back, e.g.
   mortgage address: F7paXV9FwqtWSzBV8TDuCK7burNiXXi3fR (it's your mine address too)
   back address    : FBEVSWpwD9wxPpnpYagQjwviL8kxfmYrkS
2. Mortgage BFC by bitfree-qt 
   both of mortgage address and back address are in your own wallet, so you mortgaged BFC to yourself, it's SAFE.
3. replace the address in run.sh(run.bat) by your mortgage address in step 1 (F7paXV9FwqtWSzBV8TDuCK7burNiXXi3fR)
4. start a bitfree node, set rpcuser and rpcpassword as run.sh
5. start mining with run.sh(run.bat), your reward will be in the mortgage address (F7paXV9FwqtWSzBV8TDuCK7burNiXXi3fR)


1. 首先在你的钱包中，产生两个地址：抵押地址和退回地址，如：
   抵押地址: F7paXV9FwqtWSzBV8TDuCK7burNiXXi3fR (这也是你的挖矿地址)
   退回地址: FBEVSWpwD9wxPpnpYagQjwviL8kxfmYrkS
2. 根据你的算力，使用bitfree-qt抵押相应数量的BFC，抵押地址和退回地址分别是步骤1中的两个地址（注意别写反了）
   这两个地址，全都是你自己钱包中产生的，也就是说你自己抵押给自己了，完全可以放心，非常的安全。
3. 修改挖矿脚本run.sh(run.bat)，用你的抵押地址，替换脚本中的示例地址
4. 在本机启动一个Bitfree节点，设置rpc的用户名和密码，参考run.sh
5. 执行挖矿脚本run.sh(run.bat)开始挖矿，你新挖到的BFC会存在你的抵抵地址中（新挖到的BFC，处于自由状态，你可以随意转帐)
