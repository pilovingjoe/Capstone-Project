package com.example.lorachatapp

import android.Manifest
import android.os.Bundle
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.example.lorachatapp.databinding.ActivityChannelBinding
import java.text.SimpleDateFormat
import java.util.*

import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class ChannelActivity : AppCompatActivity() {

    private lateinit var binding: ActivityChannelBinding
    private lateinit var adapter: ChatAdapter
    private val messageList = mutableListOf<Message>()
    private var senderName: String? = null

    var BLEDevice = MyApplication.getData()

    private val db by lazy { AppDatabase.getDatabase(this) }
    private val messageDao by lazy { db.messageDao() }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityChannelBinding.inflate(layoutInflater)
        setContentView(binding.root)

        senderName = intent.getStringExtra("SENDER_NAME") ?: "Group Chat"

        setupToolbar()
        setupRecyclerView()
        setupSendButton()

        loadMessages()
    }

    private fun setupToolbar() {
        setSupportActionBar(binding.toolbarGchannel)
        supportActionBar?.apply {
            title = senderName
            setDisplayHomeAsUpEnabled(true)
        }
        binding.toolbarGchannel.setNavigationOnClickListener {
            onBackPressedDispatcher.onBackPressed()
        }
    }

    private fun setupRecyclerView() {
        adapter = ChatAdapter(messageList)
        val layoutManager = LinearLayoutManager(this)
        layoutManager.stackFromEnd = true // Start from bottom
        binding.recyclerGchat.layoutManager = layoutManager
        binding.recyclerGchat.adapter = adapter
    }

    private fun loadMessages() {
        lifecycleScope.launch {
            val messages = withContext(Dispatchers.IO) {
                val dbMessages = if (senderName == "Group Chat") {
                    messageDao.getAll()
                } else {
                    // Load messages where this person is either sender or receiver
                    messageDao.loadAllBySender(senderName!!) + messageDao.loadAllByReceiver(senderName!!)
                }
                dbMessages.map { dbMsg ->
                    Message(
                        content = dbMsg.content,
                        sender = dbMsg.sender,
                        receiver = dbMsg.receiver,
                        dateTime = dbMsg.dateTime,
                        isMe = dbMsg.isMe
                    )
                }.sortedBy { it.dateTime }
            }
            messageList.clear()
            messageList.addAll(messages)
            adapter.notifyDataSetChanged()
            if (messageList.isNotEmpty()) {
                binding.recyclerGchat.scrollToPosition(messageList.size - 1)
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun setupSendButton() {
        binding.buttonGchatSend.setOnClickListener @androidx.annotation.RequiresPermission(android.Manifest.permission.BLUETOOTH_CONNECT) {
            val content = binding.editGchatMessage.text.toString().trim()
            if (content.isNotEmpty()) {
                sendMessage(content, receiver = senderName ?: "")

                binding.editGchatMessage.text.clear()
            }
        }
    }

    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    private fun sendMessage(content: String, receiver: String) {
        val datetime = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        val time = datetime.format(Date())
        val message = Message(
            content = content,
            sender = "Me",
            receiver = receiver,
            dateTime = time,
            isMe = true,
        )
        
        lifecycleScope.launch(Dispatchers.IO) {
            messageDao.insertAll(DBMessage(content = content, sender = "Me", receiver = receiver, dateTime = time, isMe = true))
        }

        messageList.add(message)
        adapter.notifyItemInserted(messageList.size - 1)
        BLEDevice?.writeMessage(message)
        binding.recyclerGchat.smoothScrollToPosition(messageList.size - 1)
    }


    @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
    fun receiveMessage() {
        // message in form of receiver;sender;dateTime;content
        val content=BLEDevice?.readMessage().toString()
        
        val rawMessage = content.split(";")
        if (rawMessage.size < 4) return

        val message = Message(
            content = rawMessage[3],
            sender = rawMessage[1],
            receiver = rawMessage[0],
            dateTime = rawMessage[2],
            isMe = false,
        )

        lifecycleScope.launch(Dispatchers.IO) {
            messageDao.insertAll(DBMessage(content = rawMessage[3], sender = rawMessage[1], receiver = rawMessage[0], dateTime = rawMessage[2], isMe = false))
        }

        if (message.sender == senderName || message.receiver == senderName || senderName == "Group Chat") {
            messageList.add(message)
            adapter.notifyItemInserted(messageList.size - 1)
            binding.recyclerGchat.smoothScrollToPosition(messageList.size - 1)
        }
    }
}

