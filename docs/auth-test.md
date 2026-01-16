```
# generate key
ssh-keygen -t ecdsa-sk -f ~/.ssh/id_ecdsa_sk -O application=ssh:test

# allow key for your user
cat ~/.ssh/id_ecdsa_sk.pub >> ~/.ssh/authorized_keys

# test login
ssh -i ~/.ssh/id_ecdsa_sk localhost

# delete key after testing
sed -i '/sk-ecdsa-sha2-nistp256@openssh.com/d' ~/.ssh/authorized_keys
```